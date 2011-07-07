function [femicrophone, aecmStructNew, enerNear, enerFar] = AECMobile(fmicrophone, afTheFarEnd, setupStruct, aecmStruct)
global NEARENDFFT;
global F;

aecmStructNew = aecmStruct;

% Magnitude spectrum of near end signal
afmicrophone = abs(fmicrophone);
%afmicrophone = NEARENDFFT(setupStruct.currentBlock,:)'/2^F(setupStruct.currentBlock,end);
% Near end energy level
ener_orig = afmicrophone'*afmicrophone;
if( ener_orig == 0)
    lowlevel = 0.01;
    afmicrophone = lowlevel*ones(size(afmicrophone));
end
%adiff = max(abs(afmicrophone - afTheFarEnd));
%if (adiff > 0)
%    disp([setupStruct.currentBlock adiff])
%end

% Store the near end energy
%aecmStructNew.enerNear(setupStruct.currentBlock) = log(afmicrophone'*afmicrophone);
aecmStructNew.enerNear(setupStruct.currentBlock) = log(sum(afmicrophone));
% Store the far end energy
%aecmStructNew.enerFar(setupStruct.currentBlock) = log(afTheFarEnd'*afTheFarEnd);
aecmStructNew.enerFar(setupStruct.currentBlock) = log(sum(afTheFarEnd));

% Update subbands (We currently use all frequency bins, hence .useSubBand is turned off)
if aecmStructNew.useSubBand
    internalIndex = 1;
    for kk=1:setupStruct.subBandLength+1
        ySubBand(kk) = mean(afmicrophone(internalIndex:internalIndex+setupStruct.numInBand(kk)-1).^aecmStructNew.bandFactor);
        xSubBand(kk) = mean(afTheFarEnd(internalIndex:internalIndex+setupStruct.numInBand(kk)-1).^aecmStructNew.bandFactor);
        internalIndex = internalIndex + setupStruct.numInBand(kk);
    end
else
    ySubBand = afmicrophone.^aecmStructNew.bandFactor;
    xSubBand = afTheFarEnd.^aecmStructNew.bandFactor;
end

% Estimated echo energy
if (aecmStructNew.bandFactor == 1)
    %aecmStructNew.enerEcho(setupStruct.currentBlock) = log((aecmStructNew.H.*xSubBand)'*(aecmStructNew.H.*xSubBand));
    %aecmStructNew.enerEchoStored(setupStruct.currentBlock) = log((aecmStructNew.HStored.*xSubBand)'*(aecmStructNew.HStored.*xSubBand));
    aecmStructNew.enerEcho(setupStruct.currentBlock) = log(sum(aecmStructNew.H.*xSubBand));
    aecmStructNew.enerEchoStored(setupStruct.currentBlock) = log(sum(aecmStructNew.HStored.*xSubBand));
elseif (aecmStructNew.bandFactor == 2)
    aecmStructNew.enerEcho(setupStruct.currentBlock) = log(aecmStructNew.H'*xSubBand);
    aecmStructNew.enerEchoStored(setupStruct.currentBlock) = log(aecmStructNew.HStored'*xSubBand);
end

% Last 100 blocks of data, used for plotting
n100 = max(1,setupStruct.currentBlock-99):setupStruct.currentBlock;
enerError = aecmStructNew.enerNear(n100)-aecmStructNew.enerEcho(n100);
enerErrorStored = aecmStructNew.enerNear(n100)-aecmStructNew.enerEchoStored(n100);

% Store the far end sub band. This is needed if we use LSE instead of NLMS
aecmStructNew.X = [xSubBand aecmStructNew.X(:,1:end-1)];

% Update energy levels, which control the VAD
if ((aecmStructNew.enerFar(setupStruct.currentBlock) < aecmStructNew.energyMin) & (aecmStructNew.enerFar(setupStruct.currentBlock) >= aecmStruct.FAR_ENERGY_MIN))
    aecmStructNew.energyMin = aecmStructNew.enerFar(setupStruct.currentBlock);
    %aecmStructNew.energyMin = max(aecmStructNew.energyMin,12);
    aecmStructNew.energyMin = max(aecmStructNew.energyMin,aecmStruct.FAR_ENERGY_MIN);
    aecmStructNew.energyLevel = (aecmStructNew.energyMax-aecmStructNew.energyMin)*aecmStructNew.energyThres+aecmStructNew.energyMin;
    aecmStructNew.energyLevelMSE = (aecmStructNew.energyMax-aecmStructNew.energyMin)*aecmStructNew.energyThresMSE+aecmStructNew.energyMin;
end
if (aecmStructNew.enerFar(setupStruct.currentBlock) > aecmStructNew.energyMax)
    aecmStructNew.energyMax = aecmStructNew.enerFar(setupStruct.currentBlock);
    aecmStructNew.energyLevel = (aecmStructNew.energyMax-aecmStructNew.energyMin)*aecmStructNew.energyThres+aecmStructNew.energyMin;
    aecmStructNew.energyLevelMSE = (aecmStructNew.energyMax-aecmStructNew.energyMin)*aecmStructNew.energyThresMSE+aecmStructNew.energyMin;
end

% Calculate current energy error in near end (estimated echo vs. near end)
dE = aecmStructNew.enerNear(setupStruct.currentBlock)-aecmStructNew.enerEcho(setupStruct.currentBlock);

%%%%%%%%
% Calculate step size used in LMS algorithm, based on current far end energy and near end energy error (dE)
%%%%%%%%
if setupStruct.stepSize_flag
    [mu, aecmStructNew] = calcStepSize(aecmStructNew.enerFar(setupStruct.currentBlock), dE, aecmStructNew, setupStruct.currentBlock, 1);
else
    mu = 0.25;
end
aecmStructNew.muLog(setupStruct.currentBlock) = mu; % Store the step size

% Estimate Echo Spectral Shape
[U, aecmStructNew.H] = fallerEstimator(ySubBand,aecmStructNew.X,aecmStructNew.H,mu);

%%%%%
% Determine if we should store or restore the channel
%%%%%
if ((setupStruct.currentBlock <= aecmStructNew.convLength) | (~setupStruct.channelUpdate_flag))
    aecmStructNew.HStored = aecmStructNew.H; % Store what you have after startup
elseif ((setupStruct.currentBlock > aecmStructNew.convLength) & (setupStruct.channelUpdate_flag))
    if ((aecmStructNew.enerFar(setupStruct.currentBlock) < aecmStructNew.energyLevelMSE) & (aecmStructNew.enerFar(setupStruct.currentBlock-1) >= aecmStructNew.energyLevelMSE))
        xxx = aecmStructNew.countMseH;
        if (xxx > 20)
            mseStored = mean(abs(aecmStructNew.enerEchoStored(setupStruct.currentBlock-xxx:setupStruct.currentBlock-1)-aecmStructNew.enerNear(setupStruct.currentBlock-xxx:setupStruct.currentBlock-1)));
            mseLatest = mean(abs(aecmStructNew.enerEcho(setupStruct.currentBlock-xxx:setupStruct.currentBlock-1)-aecmStructNew.enerNear(setupStruct.currentBlock-xxx:setupStruct.currentBlock-1)));
            %fprintf('Stored: %4f Latest: %4f\n', mseStored, mseLatest) % Uncomment if you want to display the MSE values
            if ((mseStored < 0.8*mseLatest) & (aecmStructNew.mseHStoredOld < 0.8*aecmStructNew.mseHLatestOld))
                aecmStructNew.H = aecmStructNew.HStored;
                fprintf('Restored H at block %d\n',setupStruct.currentBlock)
            elseif (((0.8*mseStored > mseLatest) & (mseLatest < aecmStructNew.mseHThreshold) & (aecmStructNew.mseHLatestOld < aecmStructNew.mseHThreshold)) | (mseStored == Inf))
                aecmStructNew.HStored = aecmStructNew.H;
                fprintf('Stored new H at block %d\n',setupStruct.currentBlock)
            end
            aecmStructNew.mseHStoredOld = mseStored;
            aecmStructNew.mseHLatestOld = mseLatest;
        end
    elseif ((aecmStructNew.enerFar(setupStruct.currentBlock) >= aecmStructNew.energyLevelMSE) & (aecmStructNew.enerFar(setupStruct.currentBlock-1) < aecmStructNew.energyLevelMSE))
        aecmStructNew.countMseH = 1;
    elseif (aecmStructNew.enerFar(setupStruct.currentBlock) >= aecmStructNew.energyLevelMSE)
        aecmStructNew.countMseH = aecmStructNew.countMseH + 1;
    end
end

%%%%%
% Check delay (calculate the delay offset (if we can))
% The algorithm is not tuned and should be used with care. It runs separately from Bastiaan's algorithm.
%%%%%
yyy = 31; % Correlation buffer length (currently unfortunately hard coded)
dxxx = 25; % Maximum offset (currently unfortunately hard coded)
if (setupStruct.currentBlock > aecmStructNew.convLength)
    if (aecmStructNew.enerFar(setupStruct.currentBlock-(yyy+2*dxxx-1):setupStruct.currentBlock) > aecmStructNew.energyLevelMSE)
        for xxx = -dxxx:dxxx
            aecmStructNew.delayLatestS(xxx+dxxx+1) = sum(sign(aecmStructNew.enerEcho(setupStruct.currentBlock-(yyy+dxxx-xxx)+1:setupStruct.currentBlock+xxx-dxxx)-mean(aecmStructNew.enerEcho(setupStruct.currentBlock-(yyy++dxxx-xxx)+1:setupStruct.currentBlock+xxx-dxxx))).*sign(aecmStructNew.enerNear(setupStruct.currentBlock-yyy-dxxx+1:setupStruct.currentBlock-dxxx)-mean(aecmStructNew.enerNear(setupStruct.currentBlock-yyy-dxxx+1:setupStruct.currentBlock-dxxx))));
        end
        aecmStructNew.newDelayCurve = 1;
    end
end
if ((setupStruct.currentBlock > 2*aecmStructNew.convLength) & ~rem(setupStruct.currentBlock,yyy*2) & aecmStructNew.newDelayCurve)
    [maxV,maxP] = max(aecmStructNew.delayLatestS);
    if ((maxP > 2) & (maxP < 2*dxxx))
        maxVLeft = aecmStructNew.delayLatestS(max(1,maxP-4));
        maxVRight = aecmStructNew.delayLatestS(min(2*dxxx+1,maxP+4));
        %fprintf('Max %d, Left %d, Right %d\n',maxV,maxVLeft,maxVRight) % Uncomment if you want to see max value
        if ((maxV > 24) & (maxVLeft < maxV - 10)  & (maxVRight < maxV - 10))
            aecmStructNew.feedbackDelay = maxP-dxxx-1;
            aecmStructNew.newDelayCurve = 0;
            aecmStructNew.feedbackDelayUpdate = 1;
            fprintf('Feedback Update at block %d\n',setupStruct.currentBlock)
        end
    end
end
% End of "Check delay"
%%%%%%%%

%%%%%
% Calculate suppression gain, based on far end energy and near end energy error (dE)
if (setupStruct.supGain_flag)
    [gamma_echo, aecmStructNew.cntIn, aecmStructNew.cntOut] = calcFilterGain(aecmStructNew.enerFar(setupStruct.currentBlock), dE, aecmStructNew, setupStruct.currentBlock, aecmStructNew.convLength, aecmStructNew.cntIn, aecmStructNew.cntOut);
else
    gamma_echo = 1;
end
aecmStructNew.gammaLog(setupStruct.currentBlock) = gamma_echo; % Store the gain
gamma_use = gamma_echo;

% Use the stored channel
U = aecmStructNew.HStored.*xSubBand;

% compute Wiener filter and suppressor function
Iy = find(ySubBand);
subBandFilter = zeros(size(ySubBand));
if (aecmStructNew.bandFactor == 2)
    subBandFilter(Iy) = (1 - gamma_use*sqrt(U(Iy)./ySubBand(Iy))); % For Faller
else
    subBandFilter(Iy) = (1 - gamma_use*(U(Iy)./ySubBand(Iy))); % For COV
end
ix0 = find(subBandFilter < 0);   % bounding trick 1
subBandFilter(ix0) = 0;
ix0 = find(subBandFilter > 1);   % bounding trick 1
subBandFilter(ix0) = 1;

% Interpolate back to normal frequency bins if we use sub bands
if aecmStructNew.useSubBand
    thefilter = interp1(setupStruct.centerFreq,subBandFilter,linspace(0,setupStruct.samplingfreq/2,setupStruct.hsupport1)','nearest');
    testfilter = interp1(setupStruct.centerFreq,subBandFilter,linspace(0,setupStruct.samplingfreq/2,1000),'nearest');
    thefilter(end) = subBandFilter(end);
    
    internalIndex = 1;
    for kk=1:setupStruct.subBandLength+1
        internalIndex:internalIndex+setupStruct.numInBand(kk)-1;
        thefilter(internalIndex:internalIndex+setupStruct.numInBand(kk)-1) = subBandFilter(kk);
        internalIndex = internalIndex + setupStruct.numInBand(kk);
    end
else
    thefilter = subBandFilter;
    testfilter = subBandFilter;
end

% Bound the filter
ix0 = find(thefilter < setupStruct.de_echo_bound);   % bounding trick 1
thefilter(ix0) = setupStruct.de_echo_bound;     % bounding trick 2
ix0 = find(thefilter > 1);   % bounding in reasonable range
thefilter(ix0) = 1;

%%%%
% NLP
%%%%
thefmean = mean(thefilter(8:16));
if (thefmean < 1)
    disp('');
end
aecmStructNew.runningfmean = setupStruct.nl_alpha*aecmStructNew.runningfmean + (1-setupStruct.nl_alpha)*thefmean;
slope0 = 1.0/(1.0 - setupStruct.nlSeverity); %
thegain = max(0.0, min(1.0, slope0*(aecmStructNew.runningfmean - setupStruct.nlSeverity)));
if ~setupStruct.nlp_flag
    thegain = 1;
end
% END NONLINEARITY
thefilter = thegain*thefilter;

%%%%
% The suppression
%%%%
femicrophone = fmicrophone .* thefilter;
% Store the output energy (used for plotting)
%aecmStructNew.enerOut(setupStruct.currentBlock) = log(abs(femicrophone)'*abs(femicrophone));
aecmStructNew.enerOut(setupStruct.currentBlock) = log(sum(abs(femicrophone)));

if aecmStructNew.plotIt
    figure(13)
    subplot(311)
    %plot(n100,enerFar(n100),'b-',n100,enerNear(n100),'k--',n100,enerEcho(n100),'r-',[n100(1) n100(end)],[1 1]*vadThNew,'b:',[n100(1) n100(end)],[1 1]*((energyMax-energyMin)/4+energyMin),'r-.',[n100(1) n100(end)],[1 1]*vadNearThNew,'g:',[n100(1) n100(end)],[1 1]*energyMax,'r-.',[n100(1) n100(end)],[1 1]*energyMin,'r-.','LineWidth',2)
    plot(n100,aecmStructNew.enerFar(n100),'b-',n100,aecmStructNew.enerNear(n100),'k--',n100,aecmStructNew.enerOut(n100),'r-.',n100,aecmStructNew.enerEcho(n100),'r-',n100,aecmStructNew.enerEchoStored(n100),'c-',[n100(1) n100(end)],[1 1]*((aecmStructNew.energyMax-aecmStructNew.energyMin)/4+aecmStructNew.energyMin),'g-.',[n100(1) n100(end)],[1 1]*aecmStructNew.energyMax,'g-.',[n100(1) n100(end)],[1 1]*aecmStructNew.energyMin,'g-.','LineWidth',2)
    %title(['Frame ',int2str(i),' av ',int2str(setupStruct.updateno),' State = ',int2str(speechState),' \mu = ',num2str(mu)])
    title(['\gamma = ',num2str(gamma_echo),' \mu = ',num2str(mu)])
    subplot(312)
    %plot(n100,enerError,'b-',[n100(1) n100(end)],[1 1]*vadNearTh,'r:',[n100(1) n100(end)],[-1.5 -1.5]*vadNearTh,'r:','LineWidth',2)
    %plot(n100,enerError,'b-',[n100(1) n100(end)],[1 1],'r:',[n100(1) n100(end)],[-2 -2],'r:','LineWidth',2)
    plot(n100,enerError,'b-',n100,enerErrorStored,'c-',[n100(1) n100(end)],[1 1]*aecmStructNew.varMean,'k--',[n100(1) n100(end)],[1 1],'r:',[n100(1) n100(end)],[-2 -2],'r:','LineWidth',2)
    % Plot mu
    %plot(n100,log2(aecmStructNew.muLog(n100)),'b-','LineWidth',2)
    %plot(n100,log2(aecmStructNew.HGain(n100)),'b-',[n100(1) n100(end)],[1 1]*log2(sum(aecmStructNew.HStored)),'r:','LineWidth',2)
    title(['Block ',int2str(setupStruct.currentBlock),' av ',int2str(setupStruct.updateno)])
    subplot(313)
    %plot(n100,enerVar(n100),'b-',[n100(1) n100(end)],[1 1],'r:',[n100(1) n100(end)],[-2 -2],'r:','LineWidth',2)
    %plot(n100,enerVar(n100),'b-','LineWidth',2)
    % Plot correlation curve

    %plot(-25:25,aecmStructNew.delayStored/max(aecmStructNew.delayStored),'c-',-25:25,aecmStructNew.delayLatest/max(aecmStructNew.delayLatest),'r-',-25:25,(max(aecmStructNew.delayStoredS)-aecmStructNew.delayStoredS)/(max(aecmStructNew.delayStoredS)-min(aecmStructNew.delayStoredS)),'c:',-25:25,(max(aecmStructNew.delayLatestS)-aecmStructNew.delayLatestS)/(max(aecmStructNew.delayLatestS)-min(aecmStructNew.delayLatestS)),'r:','LineWidth',2)
    %plot(-25:25,aecmStructNew.delayStored,'c-',-25:25,aecmStructNew.delayLatest,'r-',-25:25,(max(aecmStructNew.delayStoredS)-aecmStructNew.delayStoredS)/(max(aecmStructNew.delayStoredS)-min(aecmStructNew.delayStoredS)),'c:',-25:25,(max(aecmStructNew.delayLatestS)-aecmStructNew.delayLatestS)/(max(aecmStructNew.delayLatestS)-min(aecmStructNew.delayLatestS)),'r:','LineWidth',2)
    %plot(-25:25,aecmStructNew.delayLatest,'r-',-25:25,(50-aecmStructNew.delayLatestS)/100,'r:','LineWidth',2)
    plot(-25:25,aecmStructNew.delayLatestS,'r:','LineWidth',2)
    %plot(-25:25,aecmStructNew.delayStored,'c-',-25:25,aecmStructNew.delayLatest,'r-','LineWidth',2)
    plot(0:32,aecmStruct.HStored,'bo-','LineWidth',2)
    %title(['\gamma | In = ',int2str(aecmStructNew.muStruct.countInInterval),' | Out High = ',int2str(aecmStructNew.muStruct.countOutHighInterval),' | Out Low = ',int2str(aecmStructNew.muStruct.countOutLowInterval)])
    pause(1)
    %if ((setupStruct.currentBlock == 860) | (setupStruct.currentBlock == 420) | (setupStruct.currentBlock == 960))
    if 0%(setupStruct.currentBlock == 960)
        figure(60)
        plot(n100,aecmStructNew.enerNear(n100),'k--',n100,aecmStructNew.enerEcho(n100),'k:','LineWidth',2)
        legend('Near End','Estimated Echo')
        title('Signal Energy witH offset compensation')
        figure(61)
        subplot(211)
        stem(sign(aecmStructNew.enerNear(n100)-mean(aecmStructNew.enerNear(n100))))
        title('Near End Energy Pattern (around mean value)')
        subplot(212)
        stem(sign(aecmStructNew.enerEcho(n100)-mean(aecmStructNew.enerEcho(n100))))
        title('Estimated Echo Energy Pattern (around mean value)')
        pause
    end
    drawnow%,pause
elseif ~rem(setupStruct.currentBlock,100)
    fprintf('Block %d of %d\n',setupStruct.currentBlock,setupStruct.updateno)
end
