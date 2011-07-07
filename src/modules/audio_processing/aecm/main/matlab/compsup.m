function [emicrophone,aaa]=compsup(microphone,TheFarEnd,avtime,samplingfreq);
% microphone = microphone signal
% aaa = nonlinearity input variable
% TheFarEnd = far end signal
% avtime = interval to compute suppression from (seconds)
% samplingfreq = sampling frequency

%if(nargin==6)
%    fprintf(1,'suppress has received a delay sequence\n');
%end


Ap500=[  1.00, -4.95, 9.801, -9.70299, 4.80298005, -0.9509900499];
Bp500=[  0.662743088639636, -2.5841655608125, 3.77668102146288, -2.45182477425154, 0.596566274575251, 0.0];


Ap200=[ 1.00, -4.875, 9.50625, -9.26859375, 4.518439453125, -0.881095693359375];
Bp200=[ 0.862545460994275, -3.2832804496114, 4.67892032308828, -2.95798023879133, 0.699796870041299, 0.0];

maxDelay=0.4; %[s]
histLen=1; %[s]


% CONSTANTS THAT YOU CAN EXPERIMENT WITH
A_GAIN=10.0;	 	% for the suppress case
oversampling = 2;	% must be power of 2; minimum is 2; 4 works
% fine for support=64, but for support=128,
% 8 gives better results.
support=64; %512	% fft support (frequency resolution; at low
% settings you can hear more distortion
% (e.g. pitch that is left-over from far-end))
% 128 works well, 64 is ok)

lowlevel = mean(abs(microphone))*0.0001;

G_ol = 0;  % Use overlapping sets of estimates

% ECHO SUPPRESSION SPECIFIC PARAMETERS
suppress_overdrive=1.0;   % overdrive factor for suppression 1.4 is good
gamma_echo=1.0;           % same as suppress_overdrive but at different place
de_echo_bound=0.0;
mLim=10;                  % rank of matrix G
%limBW = 1;		  % use bandwidth-limited response for G
if mLim > (support/2+1)
    error('mLim in suppress.m too large\n');
end


dynrange=1.0000e-004;

% other, constants
hsupport = support/2;
hsupport1 = hsupport+1;
factor =  2 / oversampling;
updatel = support/oversampling;
win=sqrt(designwindow(0,support));
estLen = round(avtime * samplingfreq/updatel)

runningfmean =0.0;

mLim = floor(hsupport1/2);
V = sqrt(2/hsupport1)*cos(pi/hsupport1*(repmat((0:hsupport1-1) + 0.5, mLim, 1).* ...
    repmat((0:mLim-1)' + 0.5, 1, hsupport1)));

fprintf(1,'updatel is %5.3f s\n', updatel/samplingfreq);



bandfirst=8; bandlast=25;
dosmooth=0;  % to get rid of wavy bin counts (can be worse or better)

% compute some constants
blockLen = support/oversampling;
maxDelayb = floor(samplingfreq*maxDelay/updatel); % in blocks
histLenb = floor(samplingfreq*histLen/updatel); % in blocks

x0=TheFarEnd;
y0=microphone;


%input
tlength=min([length(microphone),length(TheFarEnd)]);
updateno=floor(tlength/updatel);
tlength=updatel*updateno;
updateno = updateno - oversampling + 1;

TheFarEnd =TheFarEnd(1:tlength);
microphone =microphone(1:tlength);

TheFarEnd =[zeros(hsupport,1);TheFarEnd(1:tlength)];
microphone =[zeros(hsupport,1);microphone(1:tlength)];


% signal length
n = min([floor(length(x0)/support)*support,floor(length(y0)/support)*support]);
nb = n/blockLen - oversampling + 1; % in blocks

% initialize space
win = sqrt([0 ; hanning(support-1)]);
sxAll2 = zeros(hsupport1,nb);
syAll2 = zeros(hsupport1,nb);

z500=zeros(5,maxDelayb+1);
z200=zeros(5,hsupport1);

bxspectrum=uint32(zeros(nb,1));
bxhist=uint32(zeros(maxDelayb+1,1));
byspectrum=uint32(zeros(nb,1));
bcount=zeros(1+maxDelayb,nb);
fcount=zeros(1+maxDelayb,nb);
fout=zeros(1+maxDelayb,nb);
delay=zeros(nb,1);
tdelay=zeros(nb,1);
nlgains=zeros(nb,1);

% create space (mainly for debugging)
emicrophone=zeros(tlength,1);
femicrophone=complex(zeros(hsupport1,updateno));
thefilter=zeros(hsupport1,updateno);
thelimiter=ones(hsupport1,updateno);
fTheFarEnd=complex(zeros(hsupport1,updateno));
afTheFarEnd=zeros(hsupport1,updateno);
fmicrophone=complex(zeros(hsupport1,updateno));
afmicrophone=zeros(hsupport1,updateno);

G = zeros(hsupport1, hsupport1);
zerovec = zeros(hsupport1,1);
zeromat = zeros(hsupport1);

% Reset sums
mmxs_a = zerovec;
mmys_a = zerovec;
s2xs_a = zerovec;
s2ys_a = zerovec;
Rxxs_a = zeromat;
Ryxs_a = zeromat;
count_a = 1;

mmxs_b = zerovec;
mmys_b = zerovec;
s2xs_b = zerovec;
s2ys_b = zerovec;
Rxxs_b = zeromat;
Ryxs_b = zeromat;
count_b = 1;

nog=0;

aaa=zeros(size(TheFarEnd));

% loop over signal blocks
fprintf(1,'.. Suppression; averaging G over %5.1f seconds; file length %5.1f seconds ..\n',avtime, length(microphone)/samplingfreq);
fprintf(1,'.. SUPPRESSING ONLY AFTER %5.1f SECONDS! ..\n',avtime);
fprintf(1,'.. 20 seconds is good ..\n');
hh = waitbar_j(0,'Please wait...');


for i=1:updateno

    sb = (i-1)*updatel + 1;
    se=sb+support-1;
    
    % analysis FFTs
    temp=fft(win .* TheFarEnd(sb:se));
    fTheFarEnd(:,i)=temp(1:hsupport1);
    xf=fTheFarEnd(:,i);
    afTheFarEnd(:,i)= abs(fTheFarEnd(:,i));
    
    temp=win .* microphone(sb:se);
    
    temp=fft(win .* microphone(sb:se));
    fmicrophone(:,i)=temp(1:hsupport1);
    yf=fmicrophone(:,i);
    afmicrophone(:,i)= abs(fmicrophone(:,i));

    
    ener_orig = afmicrophone(:,i)'*afmicrophone(:,i);
    if( ener_orig == 0)
        afmicrophone(:,i)=lowlevel*ones(size(afmicrophone(:,i)));
    end
    
    
    	% use log domain (showed improved performance)
xxf= sqrt(real(xf.*conj(xf))+1e-20);
yyf= sqrt(real(yf.*conj(yf))+1e-20);
        sxAll2(:,i) = 20*log10(xxf);
	syAll2(:,i) = 20*log10(yyf);

       mD=min(i-1,maxDelayb);
      xthreshold = sum(sxAll2(:,i-mD:i),2)/(maxDelayb+1);

      [yout, z200] = filter(Bp200,Ap200,syAll2(:,i),z200,2);
      yout=yout/(maxDelayb+1);
      ythreshold = mean(syAll2(:,i-mD:i),2);
      

  bxspectrum(i)=getBspectrum(sxAll2(:,i),xthreshold,bandfirst,bandlast);
  byspectrum(i)=getBspectrum(syAll2(:,i),yout,bandfirst,bandlast);

  bxhist(end-mD:end)=bxspectrum(i-mD:i);
  
  bcount(:,i)=hisser2( ...
     byspectrum(i),flipud(bxhist),bandfirst,bandlast);
 
 
  [fout(:,i), z500] = filter(Bp500,Ap500,bcount(:,i),z500,2);
  fcount(:,i)=sum(bcount(:,max(1,i-histLenb+1):i),2); % using the history range
 fout(:,i)=round(fout(:,i)); 
  [value,delay(i)]=min(fout(:,i),[],1);
  tdelay(i)=(delay(i)-1)*support/(samplingfreq*oversampling);

    % compensate

    idel =  max(i - delay(i) + 1,1);
    
  
    % echo suppression
    
    noisyspec = afmicrophone(:,i);
    
    % Estimate G using covariance matrices
    
    % Cumulative estimates    
    xx = afTheFarEnd(:,idel);
    yy = afmicrophone(:,i);
    
    % Means
    mmxs_a = mmxs_a + xx;
    mmys_a = mmys_a + yy;
    if (G_ol)
        mmxs_b = mmxs_b + xx;  
        mmys_b = mmys_b + yy;
        mmy = mean([mmys_a/count_a mmys_b/count_b],2);
        mmx = mean([mmxs_a/count_a mmxs_b/count_b],2);
    else
        mmx = mmxs_a/count_a;   
        mmy = mmys_a/count_a;   
    end
    count_a = count_a + 1;
    count_b = count_b + 1;
    
    % Mean removal
    xxm = xx - mmx;
    yym = yy - mmy;
    
    % Variances
    s2xs_a = s2xs_a +  xxm .* xxm;
    s2ys_a = s2ys_a +  yym .* yym;
    s2xs_b = s2xs_b +  xxm .* xxm;
    s2ys_b = s2ys_b +  yym .* yym;
    
    % Correlation matrices  
    Rxxs_a = Rxxs_a + xxm * xxm';
    Ryxs_a = Ryxs_a + yym * xxm';
    Rxxs_b = Rxxs_b + xxm * xxm';
    Ryxs_b = Ryxs_b + yym * xxm';
    
    
    % Gain matrix A
    
    if mod(i, estLen) == 0
        
        
        % Cumulative based estimates
        Rxxf = Rxxs_a / (estLen - 1);
        Ryxf = Ryxs_a / (estLen - 1);
        
        % Variance normalization
        s2x2 = s2xs_a / (estLen - 1);
        s2x2 = sqrt(s2x2);
       % Sx = diag(max(s2x2,dynrange*max(s2x2)));
        Sx = diag(s2x2);
        if (sum(s2x2) > 0)
          iSx = inv(Sx);
         else
                 iSx= Sx + 0.01;
         end
             
        s2y2 = s2ys_a / (estLen - 1);
        s2y2 = sqrt(s2y2);
       % Sy = diag(max(s2y2,dynrange*max(s2y2)));
        Sy = diag(s2y2);
        iSy = inv(Sy);        
        rx = iSx * Rxxf * iSx;
        ryx = iSy * Ryxf * iSx;
        
     
        
        dbd= 7; % Us less than the full matrix
        
        % k x m
        % Bandlimited structure on G
        LSEon = 0; % Default is using MMSE
        if (LSEon)
            ryx = ryx*rx;
            rx = rx*rx;
        end
        p = dbd-1;
        gaj = min(min(hsupport1,2*p+1),min([p+(1:hsupport1); hsupport1+p+1-(1:hsupport1)]));
        cgaj = [0 cumsum(gaj)];
        
        G3 = zeros(hsupport1);
        for kk=1:hsupport1
            ki = max(0,kk-p-1);
            if (sum(sum(rx(ki+1:ki+gaj(kk),ki+1:ki+gaj(kk))))>0)
               G3(kk,ki+1:ki+gaj(kk)) = ryx(kk,ki+1:ki+gaj(kk))/rx(ki+1:ki+gaj(kk),ki+1:ki+gaj(kk));
           else
               G3(kk,ki+1:ki+gaj(kk)) = ryx(kk,ki+1:ki+gaj(kk));
           end
        end
        % End Bandlimited structure
        
        G = G3;
        G(abs(G)<0.01)=0;
        G = suppress_overdrive * Sy * G * iSx;
        
        if 1
            figure(32); mi=2;
            surf(max(min(G,mi),-mi)); view(2)
            title('Unscaled Masked Limited-bandwidth G');
        end
        pause(0.05);
        
        % Reset sums
        mmxs_a = zerovec;
        mmys_a = zerovec;
        s2xs_a = zerovec;
        s2ys_a = zerovec;
        Rxxs_a = zeromat;
        Ryxs_a = zeromat;
        count_a = 1;
        
    end
    
    if (G_ol)    
        % Gain matrix B
        
        if ((mod((i-estLen/2), estLen) == 0) & i>estLen)
            
            
            % Cumulative based estimates
            Rxxf = Rxxs_b / (estLen - 1);
            Ryxf = Ryxs_b / (estLen - 1);
            
            % Variance normalization
            s2x2 = s2xs_b / (estLen - 1);
            s2x2 = sqrt(s2x2);
            Sx = diag(max(s2x2,dynrange*max(s2x2)));
            iSx = inv(Sx);
            s2y2 = s2ys_b / (estLen - 1);
            s2y2 = sqrt(s2y2);
            Sy = diag(max(s2y2,dynrange*max(s2y2)));
            iSy = inv(Sy);        
            rx = iSx * Rxxf * iSx;
            ryx = iSy * Ryxf * iSx;
            
            
            % Bandlimited structure on G
            LSEon = 0; % Default is using MMSE
            if (LSEon)
                ryx = ryx*rx;
                rx = rx*rx;
            end
            p = dbd-1;
            gaj = min(min(hsupport1,2*p+1),min([p+(1:hsupport1); hsupport1+p+1-(1:hsupport1)]));
            cgaj = [0 cumsum(gaj)];
            
            G3 = zeros(hsupport1);
            for kk=1:hsupport1
                ki = max(0,kk-p-1);
                G3(kk,ki+1:ki+gaj(kk)) = ryx(kk,ki+1:ki+gaj(kk))/rx(ki+1:ki+gaj(kk),ki+1:ki+gaj(kk));
            end
            % End Bandlimited structure
            
            G = G3;
            G(abs(G)<0.01)=0;
            G = suppress_overdrive * Sy * G * iSx;
            
            if 1
                figure(32); mi=2;
                surf(max(min(G,mi),-mi)); view(2)
                title('Unscaled Masked Limited-bandwidth G');
            end
            pause(0.05);
            
            
            % Reset sums
            mmxs_b = zerovec;
            mmys_b = zerovec;
            s2xs_b = zerovec;
            s2ys_b = zerovec;
            Rxxs_b = zeromat;
            Ryxs_b = zeromat;
            count_b = 1;
            
        end
        
    end
    
    FECestimate2 = G*afTheFarEnd(:,idel);
    
    % compute Wiener filter and suppressor function
    thefilter(:,i) = (noisyspec - gamma_echo*FECestimate2) ./ noisyspec;
    ix0 = find(thefilter(:,i)<de_echo_bound);   % bounding trick 1
    thefilter(ix0,i) = de_echo_bound;     % bounding trick 2
    ix0 = find(thefilter(:,i)>1);   % bounding in reasonable range
    thefilter(ix0,i) = 1;
    
    % NONLINEARITY
    nl_alpha=0.8;    % memory; seems not very critical
    nlSeverity=0.3;  % nonlinearity severity: 0 does nothing; 1 suppresses all
    thefmean=mean(thefilter(8:16,i));
    if (thefmean<1)
        disp('');
    end
    runningfmean = nl_alpha*runningfmean + (1-nl_alpha)*thefmean;
    aaa(sb+20+1:sb+20+updatel)=10000*runningfmean* ones(updatel,1); % debug
    slope0=1.0/(1.0-nlSeverity); %
    thegain = max(0.0,min(1.0,slope0*(runningfmean-nlSeverity)));
    % END NONLINEARITY
    thefilter(:,i) = thegain*thefilter(:,i);
    
    
    % Wiener filtering
    femicrophone(:,i) = fmicrophone(:,i) .* thefilter(:,i);
    thelimiter(:,i) = (noisyspec - A_GAIN*FECestimate2) ./ noisyspec;
    index = find(thelimiter(:,i)>1.0);
    thelimiter(index,i) = 1.0;
    index = find(thelimiter(:,i)<0.0);
    thelimiter(index,i) = 0.0;
    
    if (rem(i,floor(updateno/20))==0)
        fprintf(1,'.');
    end
    if mod(i,50)==0
        waitbar_j(i/updateno,hh); 
    end
    
    
    % reconstruction; first make spectrum odd
    temp=[femicrophone(:,i);flipud(conj(femicrophone(2:hsupport,i)))];
    emicrophone(sb:se) = emicrophone(sb:se) + factor * win .* real(ifft(temp));

end
fprintf(1,'\n');

close(hh);