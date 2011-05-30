function [mu, aecmStructNew] = calcStepSize(energy, dE, aecmStruct, t, logscale)

if (nargin < 4)
    t = 1;
    logscale = 1;
elseif (nargin == 4)
    logscale = 1;
end
T = aecmStruct.convLength;

if logscale
    currentMuMax = aecmStruct.MU_MIN + (aecmStruct.MU_MAX-aecmStruct.MU_MIN)*min(t,T)/T;
    if (aecmStruct.energyMin >= aecmStruct.energyMax)
        mu = aecmStruct.MU_MIN;
    else
        mu = (energy - aecmStruct.energyMin)/(aecmStruct.energyMax - aecmStruct.energyMin)*(currentMuMax-aecmStruct.MU_MIN) + aecmStruct.MU_MIN;
    end
    mu = 2^mu;
    if (energy < aecmStruct.energyLevel)
        mu = 0;
    end
else
    muMin = 0;
    muMax = 0.5;
    currentMuMax = muMin + (muMax-muMin)*min(t,T)/T;
    if (aecmStruct.energyMin >= aecmStruct.energyMax)
        mu = muMin;
    else
        mu = (energy - aecmStruct.energyMin)/(aecmStruct.energyMax - aecmStruct.energyMin)*(currentMuMax-muMin) + muMin;
    end
end
dE2 = 1;
dEOffset = -0.5;
offBoost = 5;
if (mu > 0)
    if (abs(dE-aecmStruct.ENERGY_DEV_OFFSET) > aecmStruct.ENERGY_DEV_TOL)
        aecmStruct.muStruct.countInInterval = 0;
    else
        aecmStruct.muStruct.countInInterval = aecmStruct.muStruct.countInInterval + 1;
    end
    if (dE < aecmStruct.ENERGY_DEV_OFFSET - aecmStruct.ENERGY_DEV_TOL)
        aecmStruct.muStruct.countOutLowInterval = aecmStruct.muStruct.countOutLowInterval + 1;
    else
        aecmStruct.muStruct.countOutLowInterval = 0;
    end
    if (dE > aecmStruct.ENERGY_DEV_OFFSET + aecmStruct.ENERGY_DEV_TOL)
        aecmStruct.muStruct.countOutHighInterval = aecmStruct.muStruct.countOutHighInterval + 1;
    else
        aecmStruct.muStruct.countOutHighInterval = 0;
    end
end
muVar = 2^min(-3,5/50*aecmStruct.muStruct.countInInterval-3);
muOff = 2^max(offBoost,min(0,offBoost*(aecmStruct.muStruct.countOutLowInterval-aecmStruct.muStruct.minOutLowInterval)/(aecmStruct.muStruct.maxOutLowInterval-aecmStruct.muStruct.minOutLowInterval)));

muLow = 1/64;
muVar = 1;
if (t < 2*T)
    muDT = 1;
    muVar = 1;
    mdEVec = 0;
    numCross = 0;
else
    muDT = min(1,max(muLow,1-(1-muLow)*(dE-aecmStruct.ENERGY_DEV_OFFSET)/aecmStruct.ENERGY_DEV_TOL));
    dEVec = aecmStruct.enerNear(t-63:t)-aecmStruct.enerEcho(t-63:t);
    %dEVec = aecmStruct.enerNear(t-20:t)-aecmStruct.enerEcho(t-20:t);
    numCross = 0;
    currentState = 0;
    for ii=1:64
        if (currentState == 0)
            currentState = (dEVec(ii) > dE2) - (dEVec(ii) < -2);
        elseif ((currentState == 1) & (dEVec(ii) < -2))
            numCross = numCross + 1;
            currentState = -1;
        elseif ((currentState == -1) & (dEVec(ii) > dE2))
            numCross = numCross + 1;
            currentState = 1;
        end
    end
            
    %logicDEVec = (dEVec > dE2) - (dEVec < -2);
    %numCross = sum(abs(diff(logicDEVec)));
    %mdEVec = mean(abs(dEVec-dEOffset));
    %mdEVec = mean(abs(dEVec-mean(dEVec)));
    %mdEVec = max(dEVec)-min(dEVec);
    %if (mdEVec > 4)%1.5)
    %    muVar = 0;
    %end
    muVar = 2^(-floor(numCross/2));
    muVar = 2^(-numCross);
end
%muVar = 1;


% if (eStd > (dE2-dEOffset))
%     muVar = 1/8;
% else
%     muVar = 1;
% end

%mu = mu*muDT*muVar*muOff;
mu = mu*muDT*muVar;
mu = min(mu,0.25);
aecmStructNew = aecmStruct;
%aecmStructNew.varMean = mdEVec;
aecmStructNew.varMean = numCross;
