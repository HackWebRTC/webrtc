function [gam, cntIn2, cntOut2] = calcFilterGain(energy, dE, aecmStruct, t, T, cntIn, cntOut)

defaultLevel = 1.2;
cntIn2 = cntIn;
cntOut2 = cntOut;
if (t < T)
    gam = 1;
else
    dE1 = -5;
    dE2 = 1;
    gamMid = 0.2;
    gam = max(0,min((energy - aecmStruct.energyMin)/(aecmStruct.energyLevel - aecmStruct.energyMin), 1-(1-gamMid)*(aecmStruct.energyMax-energy)/(aecmStruct.energyMax-aecmStruct.energyLevel)));
    
    dEOffset = -0.5;
    dEWidth = 1.5;
    %gam2 = max(1,2-((dE-dEOffset)/(dE2-dEOffset)).^2);
    gam2 = 1+(abs(dE-dEOffset)<(dE2-dEOffset));
    
    gam = gam*gam2;
    
    
    if (energy < aecmStruct.energyLevel)
        gam = 0;
    else
        gam = defaultLevel;
    end
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
    gam = max(0, gam - numCross/25);
    gam = 1;
    
    ener_A = 1;
    ener_B = 0.8;
    ener_C = aecmStruct.energyLevel + (aecmStruct.energyMax-aecmStruct.energyLevel)/5;
    dE_A = 4;%2;
    dE_B = 3.6;%1.8;
    dE_C = 0.9*dEWidth;
    dE_D = 1;
    timeFactorLength = 10;
    ddE = abs(dE-dEOffset);
    if (energy < aecmStruct.energyLevel)
        gam = 0;
    else
        gam = 1;
        gam2 = max(0, min(ener_B*(energy-aecmStruct.energyLevel)/(ener_C-aecmStruct.energyLevel), ener_B+(ener_A-ener_B)*(energy-ener_C)/(aecmStruct.energyMax-ener_C)));
        if (ddE < dEWidth)
            % Update counters
            cntIn2 = cntIn2 + 1;
            if (cntIn2 > 2)
                cntOut2 = 0;
            end
            gam3 = max(dE_D, min(dE_A-(dE_A-dE_B)*(ddE/dE_C), dE_D+(dE_B-dE_D)*(dEWidth-ddE)/(dEWidth-dE_C)));
            gam3 = dE_A;
        else
            % Update counters
            cntOut2 = cntOut2 + 1;
            if (cntOut2 > 2)
                cntIn2 = 0;
            end
            %gam2 = 1;
            gam3 = dE_D;
        end
        timeFactor = min(1, cntIn2/timeFactorLength);
        gam = gam*(1-timeFactor) + timeFactor*gam2*gam3;
    end
    %gam = gam/floor(numCross/2+1);
end
if isempty(gam)
    numCross
    timeFactor
    cntIn2
    cntOut2
    gam2
    gam3
end
