useHTC = 1; % Set this if you want to run a single file and set file names below. Otherwise use simEnvironment to run from several scenarios in a row
delayCompensation_flag = 0; % Set this flag to one if you want to turn on the delay compensation/enhancement
global FARENDFFT;
global NEARENDFFT;
global F;

if useHTC
%    fid=fopen('./htcTouchHd/nb/aecFar.pcm'); xFar=fread(fid,'short'); fclose(fid);
%    fid=fopen('./htcTouchHd/nb/aecNear.pcm'); yNear=fread(fid,'short'); fclose(fid);
%    fid=fopen('./samsungBlackjack/nb/aecFar.pcm'); xFar=fread(fid,'short'); fclose(fid);
%    fid=fopen('./samsungBlackjack/nb/aecNear.pcm'); yNear=fread(fid,'short'); fclose(fid);
%     fid=fopen('aecFarPoor.pcm'); xFar=fread(fid,'short'); fclose(fid);
%     fid=fopen('aecNearPoor.pcm'); yNear=fread(fid,'short'); fclose(fid);
%     fid=fopen('out_aes.pcm'); outAES=fread(fid,'short'); fclose(fid);
   fid=fopen('aecFar4.pcm'); xFar=fread(fid,'short'); fclose(fid);
   fid=fopen('aecNear4.pcm'); yNear=fread(fid,'short'); fclose(fid);
    yNearSpeech = zeros(size(xFar));
     fs = 8000;
     frameSize = 64;
%     frameSize = 128;
     fs = 16000;
%     frameSize = 256;
%F = load('fftValues.txt');
%FARENDFFT = F(:,1:33);
%NEARENDFFT = F(:,34:66);

else
    loadFileFar = [speakerType, '_s_',scenario,'_far_b.wav'];
    [xFar,fs,nbits] = wavread(loadFileFar);
    xFar = xFar*2^(nbits-1);
    loadFileNear = [speakerType, '_s_',scenario,'_near_b.wav'];
    [yNear,fs,nbits] = wavread(loadFileNear);
    yNear = yNear*2^(nbits-1);
    loadFileNearSpeech = [speakerType, '_s_',scenario,'_nearSpeech_b.wav'];
    [yNearSpeech,fs,nbits] = wavread(loadFileNearSpeech);
    yNearSpeech = yNearSpeech*2^(nbits-1);
    frameSize = 256;
end

dtRegions = [];

% General settings for the AECM
setupStruct = struct(...
    'stepSize_flag', 1,...      % This flag turns on the step size calculation. If turned off, mu = 0.25.
    'supGain_flag', 0,...       % This flag turns on the suppression gain calculation. If turned off, gam = 1.
    'channelUpdate_flag', 0,... % This flag turns on the channel update. If turned off, H is updated for convLength and then kept constant.
    'nlp_flag', 0,...           % Turn on/off NLP
    'withVAD_flag', 0,...           % Turn on/off NLP
    'useSubBand', 0,...         % Set to 1 if to use subBands
    'useDelayEstimation', 1,... % Set to 1 if to use delay estimation
    'support', frameSize,...    % # of samples per frame
    'samplingfreq',fs,...       % Sampling frequency
    'oversampling', 2,...       % Overlap between blocks/frames
    'updatel', 0,...            % # of samples between blocks
    'hsupport1', 0,...          % # of bins in frequency domain
    'factor', 0,...             % synthesis window amplification
    'tlength', 0,...            % # of samples of entire file
    'updateno', 0,...           % # of updates
    'nb', 1,...                 % # of blocks
    'currentBlock', 0,...       %
    'win', zeros(frameSize,1),...% Window to apply for fft and synthesis
    'avtime', 1,...             % Time (in sec.) to perform averaging
    'estLen', 0,...             % Averaging in # of blocks
    'A_GAIN', 10.0,...          % 
    'suppress_overdrive', 1.0,...   % overdrive factor for suppression 1.4 is good
    'gamma_echo', 1.0,...       % same as suppress_overdrive but at different place
    'de_echo_bound', 0.0,...    %
    'nl_alpha', 0.4,...         % memory; seems not very critical
    'nlSeverity', 0.2,...         % nonlinearity severity: 0 does nothing; 1 suppresses all
    'numInBand', [],...         % # of frequency bins in resp. subBand
    'centerFreq', [],...        % Center frequency of resp. subBand
    'dtRegions', dtRegions,...  % Regions where we have DT
    'subBandLength', frameSize/2);%All bins
    %'subBandLength', 11);       %Something's wrong when subBandLength even
    %'nl_alpha', 0.8,...         % memory; seems not very critical

delayStruct = struct(...
    'bandfirst', 8,...
    'bandlast', 25,...
    'smlength', 600,...
    'maxDelay', 0.4,...
    'oneGoodEstimate', 0,...
    'delayAdjust', 0,...
    'maxDelayb', 0);
% More parameters in delayStruct are constructed in "updateSettings" below

% Make struct settings
[setupStruct, delayStruct] = updateSettings(yNear, xFar, setupStruct, delayStruct);
setupStruct.numInBand = ones(setupStruct.hsupport1,1);

Q = 1; % Time diversity in channel
% General settings for the step size calculation
muStruct = struct(...
    'countInInterval', 0,...
    'countOutHighInterval', 0,...
    'countOutLowInterval', 0,...
    'minInInterval', 50,...
    'minOutHighInterval', 10,...
    'minOutLowInterval', 10,...
    'maxOutLowInterval', 50);
% General settings for the AECM
aecmStruct = struct(...
    'plotIt', 0,... % Set to 0 to turn off plotting
    'useSubBand', 0,...
    'bandFactor', 1,...
    'H', zeros(setupStruct.subBandLength+1,Q),...
    'HStored', zeros(setupStruct.subBandLength+1,Q),...
    'X', zeros(setupStruct.subBandLength+1,Q),...
    'energyThres', 0.28,...
    'energyThresMSE', 0.4,...
    'energyMin', inf,...
    'energyMax', -inf,...
    'energyLevel', 0,...
    'energyLevelMSE', 0,...
    'convLength', 100,...
    'gammaLog', ones(setupStruct.updateno,1),...
    'muLog', ones(setupStruct.updateno,1),...
    'enerFar', zeros(setupStruct.updateno,1),...
    'enerNear', zeros(setupStruct.updateno,1),...
    'enerEcho', zeros(setupStruct.updateno,1),...
    'enerEchoStored', zeros(setupStruct.updateno,1),...
    'enerOut', zeros(setupStruct.updateno,1),...
    'runningfmean', 0,...
    'muStruct', muStruct,...
    'varMean', 0,...
    'countMseH', 0,...
    'mseHThreshold', 1.1,...
    'mseHStoredOld', inf,...
    'mseHLatestOld', inf,...
    'delayLatestS', zeros(1,51),...
    'feedbackDelay', 0,...
    'feedbackDelayUpdate', 0,...
    'cntIn', 0,...
    'cntOut', 0,...
    'FAR_ENERGY_MIN', 1,...
    'ENERGY_DEV_OFFSET', 0.5,...
    'ENERGY_DEV_TOL', 1.5,...
    'MU_MIN', -16,...
    'MU_MAX', -2,...
    'newDelayCurve', 0);

% Adjust speech signals
xFar = [zeros(setupStruct.hsupport1-1,1);xFar(1:setupStruct.tlength)];
yNear = [zeros(setupStruct.hsupport1-1,1);yNear(1:setupStruct.tlength)];
yNearSpeech = [zeros(setupStruct.hsupport1-1,1);yNearSpeech(1:setupStruct.tlength)];
xFar = xFar(1:setupStruct.tlength);
yNear = yNear(1:setupStruct.tlength);

% Set figure settings
if aecmStruct.plotIt
    figure(13)
    set(gcf,'doublebuffer','on')
end
%%%%%%%%%%
% Here starts the algorithm
% Dividing into frames and then estimating the near end speech
%%%%%%%%%%
fTheFarEnd      = complex(zeros(setupStruct.hsupport1,1));
afTheFarEnd     = zeros(setupStruct.hsupport1,setupStruct.updateno+1);
fFar            = zeros(setupStruct.hsupport1,setupStruct.updateno+1);
fmicrophone     = complex(zeros(setupStruct.hsupport1,1));
afmicrophone    = zeros(setupStruct.hsupport1,setupStruct.updateno+1);
fNear           = zeros(setupStruct.hsupport1,setupStruct.updateno+1);
femicrophone    = complex(zeros(setupStruct.hsupport1,1));
emicrophone     = zeros(setupStruct.tlength,1);

if (setupStruct.useDelayEstimation == 2)
    delSamples = [1641 1895 2032 1895 2311 2000 2350 2222 NaN 2332 2330 2290 2401 2415 NaN 2393 2305 2381 2398];
    delBlocks = round(delSamples/setupStruct.updatel);
    delStarts = floor([25138 46844 105991 169901 195739 218536 241803 333905 347703 362660 373753 745135 765887 788078 806257 823835 842443 860139 881869]/setupStruct.updatel);
else
    delStarts = [];
end

for i=1:setupStruct.updateno
    setupStruct.currentBlock = i;
    
    sb = (i-1)*setupStruct.updatel + 1;
    se = sb + setupStruct.support - 1;
    
    %%%%%%%
    % Analysis FFTs
    %%%%%%%
    % Far end signal
    temp = fft(setupStruct.win .* xFar(sb:se))/frameSize;
    fTheFarEnd = temp(1:setupStruct.hsupport1);
    afTheFarEnd(:,i) = abs(fTheFarEnd);
    fFar(:,i) = fTheFarEnd;
    % Near end signal
    temp = fft(setupStruct.win .* yNear(sb:se))/frameSize;%,pause
    fmicrophone = temp(1:setupStruct.hsupport1);
    afmicrophone(:,i) = abs(fmicrophone);
    fNear(:,i) = fmicrophone;
    %abs(fmicrophone),pause
    % The true near end speaker (if we have such info)
    temp = fft(setupStruct.win .* yNearSpeech(sb:se));
    aftrueSpeech = abs(temp(1:setupStruct.hsupport1));
    
    if(i == 1000)
        %break;
    end
    
    % Perform delay estimation
    if (setupStruct.useDelayEstimation == 1)
        % Delay Estimation
        delayStruct = align(fTheFarEnd, fmicrophone, delayStruct, i);
        %delayStruct.delay(i) = 39;%19;
        idel =  max(i - delayStruct.delay(i) + 1,1);
        
        if delayCompensation_flag
            % If we have a new delay estimate from Bastiaan's alg. update the offset
            if (delayStruct.delay(i) ~= delayStruct.delay(max(1,i-1)))
                delayStruct.delayAdjust = delayStruct.delayAdjust + delayStruct.delay(i) - delayStruct.delay(i-1);
            end
            % Store the compensated delay
            delayStruct.delayNew(i) = delayStruct.delay(i) - delayStruct.delayAdjust;
            if (delayStruct.delayNew(i) < 1)
                % Something's wrong
                pause,break
            end
            % Compensate with the offset estimate
            idel = idel + delayStruct.delayAdjust;
        end
        if 0%aecmStruct.plotIt
            figure(1)
            plot(1:i,delayStruct.delay(1:i),'k:',1:i,delayStruct.delayNew(1:i),'k--','LineWidth',2),drawnow
        end
    elseif (setupStruct.useDelayEstimation == 2)
        % Use "manual delay"
        delIndex = find(delStarts<i);
        if isempty(delIndex)
            idel = i;
        else
            idel = i - delBlocks(max(delIndex));
            if isnan(idel)
                idel = i - delBlocks(max(delIndex)-1);
            end
        end
    else
        % No delay estimation
        %idel = max(i - 18, 1);
        idel = max(i - 50, 1);
    end

    %%%%%%%%
    % This is the AECM algorithm
    %
    % Output is the new frequency domain signal (hopefully) echo compensated
    %%%%%%%%
    [femicrophone, aecmStruct] = AECMobile(fmicrophone, afTheFarEnd(:,idel), setupStruct, aecmStruct);
    %[femicrophone, aecmStruct] = AECMobile(fmicrophone, FARENDFFT(idel,:)'/2^F(idel,end-1), setupStruct, aecmStruct);
    
    if aecmStruct.feedbackDelayUpdate
        % If the feedback tells us there is a new offset out there update the enhancement
        delayStruct.delayAdjust = delayStruct.delayAdjust + aecmStruct.feedbackDelay;
        aecmStruct.feedbackDelayUpdate = 0;
    end
    
    % reconstruction; first make spectrum odd
    temp = [femicrophone; flipud(conj(femicrophone(2:(setupStruct.hsupport1-1))))];
    emicrophone(sb:se) = emicrophone(sb:se) + setupStruct.factor * setupStruct.win .* real(ifft(temp))*frameSize;
    if max(isnan(emicrophone(sb:se)))
        % Something's wrong with the output at block i
        i
        break
    end
end


if useHTC
    fid=fopen('aecOutMatlabC.pcm','w');fwrite(fid,int16(emicrophone),'short');fclose(fid);
    %fid=fopen('farendFFT.txt','w');fwrite(fid,int16(afTheFarEnd(:)),'short');fclose(fid);
    %fid=fopen('farendFFTreal.txt','w');fwrite(fid,int16(imag(fFar(:))),'short');fclose(fid);
    %fid=fopen('farendFFTimag.txt','w');fwrite(fid,int16(real(fFar(:))),'short');fclose(fid);
    %fid=fopen('nearendFFT.txt','w');fwrite(fid,int16(afmicrophone(:)),'short');fclose(fid);
    %fid=fopen('nearendFFTreal.txt','w');fwrite(fid,int16(real(fNear(:))),'short');fclose(fid);
    %fid=fopen('nearendFFTimag.txt','w');fwrite(fid,int16(imag(fNear(:))),'short');fclose(fid);
end
if useHTC
    %spclab(setupStruct.samplingfreq,xFar,yNear,emicrophone)
else
    spclab(setupStruct.samplingfreq,xFar,yNear,emicrophone,yNearSpeech)
end    
