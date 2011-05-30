function [setupStructNew, delayStructNew] = updateSettings(microphone, TheFarEnd, setupStruct, delayStruct);

% other, constants
setupStruct.hsupport1 = setupStruct.support/2 + 1;
setupStruct.factor =  2 / setupStruct.oversampling;
setupStruct.updatel = setupStruct.support/setupStruct.oversampling;
setupStruct.estLen = round(setupStruct.avtime * setupStruct.samplingfreq/setupStruct.updatel);

% compute some constants
blockLen = setupStruct.support/setupStruct.oversampling;
delayStruct.maxDelayb = floor(setupStruct.samplingfreq*delayStruct.maxDelay/setupStruct.updatel); % in blocks

%input
tlength = min([length(microphone),length(TheFarEnd)]);
updateno = floor(tlength/setupStruct.updatel);
setupStruct.tlength = setupStruct.updatel*updateno;
setupStruct.updateno = updateno - setupStruct.oversampling + 1;

% signal length
n = floor(min([length(TheFarEnd), length(microphone)])/setupStruct.support)*setupStruct.support;
setupStruct.nb = n/blockLen - setupStruct.oversampling + 1; % in blocks

setupStruct.win = sqrt([0 ; hanning(setupStruct.support-1)]);

% Construct filterbank in Bark-scale

K = setupStruct.subBandLength; %Something's wrong when K even
erbs = 21.4*log10(0.00437*setupStruct.samplingfreq/2+1);
fe = (10.^((0:K)'*erbs/K/21.4)-1)/0.00437;
setupStruct.centerFreq = fe;
H = diag(ones(1,K-1))+diag(ones(1,K-2),-1);
Hinv = inv(H);
aty = 2*Hinv(end,:)*fe(2:end-1);
boundary = aty - (setupStruct.samplingfreq/2 + fe(end-1))/2;
if rem(K,2)
    x1 = min([fe(2)/2, -boundary]);
else
    x1 = max([0, boundary]);
end
%x1
g = fe(2:end-1);
g(1) = g(1) - x1/2;
x = 2*Hinv*g;
x = [x1;x];
%figure(42), clf
xy = zeros((K+1)*4,1);
yy = zeros((K+1)*4,1);
xy(1:4) = [fe(1) fe(1) x(1) x(1)]';
yy(1:4) = [0 1 1 0]'/x(1);
for kk=2:K
    xy((kk-1)*4+(1:4)) = [x(kk-1) x(kk-1) x(kk) x(kk)]';
    yy((kk-1)*4+(1:4)) = [0 1 1 0]'/(x(kk)-x(kk-1));
end
xy(end-3:end) = [x(K) x(K) fe(end) fe(end)]';
yy(end-3:end) = [0 1 1 0]'/(fe(end)*2-2*x(K));
%plot(xy,yy,'LineWidth',2)
%fill(xy,yy,'y')

x = [0;x];
xk = x*setupStruct.hsupport1/setupStruct.samplingfreq*2;
%setupStruct.erbBoundaries = xk;
numInBand = zeros(length(xk),1);
xh = (0:setupStruct.hsupport1-1);

for kk=1:length(xk)
    if (kk==length(xk))
        numInBand(kk) = length(find(xh>=xk(kk)));
    else
        numInBand(kk) = length(intersect(find(xh>=xk(kk)),find(xh<xk(kk+1))));
    end
end
setupStruct.numInBand = numInBand;

setupStructNew = setupStruct;

delayStructNew = struct(...
    'sxAll2',zeros(setupStructNew.hsupport1,setupStructNew.nb),...
    'syAll2',zeros(setupStructNew.hsupport1,setupStructNew.nb),...
    'z200',zeros(5,setupStructNew.hsupport1),...
    'z500',zeros(5,delayStruct.maxDelayb+1),...
    'bxspectrum',uint32(zeros(setupStructNew.nb,1)),...
    'byspectrum',uint32(zeros(setupStructNew.nb,1)),...
    'bandfirst',delayStruct.bandfirst,'bandlast',delayStruct.bandlast,...
    'bxhist',uint32(zeros(delayStruct.maxDelayb+1,1)),...
    'bcount',zeros(1+delayStruct.maxDelayb,setupStructNew.nb),...
    'fout',zeros(1+delayStruct.maxDelayb,setupStructNew.nb),...
    'new',zeros(1+delayStruct.maxDelayb,setupStructNew.nb),...
    'smlength',delayStruct.smlength,...
    'maxDelay', delayStruct.maxDelay,...
    'maxDelayb', delayStruct.maxDelayb,...
    'oneGoodEstimate', 0,...
    'delayAdjust', 0,...
    'delayNew',zeros(setupStructNew.nb,1),...
    'delay',zeros(setupStructNew.nb,1));
