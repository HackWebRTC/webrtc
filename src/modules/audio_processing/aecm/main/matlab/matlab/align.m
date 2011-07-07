function [delayStructNew] = align(xf, yf, delayStruct, i, trueDelay);

%%%%%%%
% Bastiaan's algorithm copied
%%%%%%%
Ap500 = [1.00, -4.95, 9.801, -9.70299, 4.80298005, -0.9509900499];
Bp500 = [0.662743088639636, -2.5841655608125, 3.77668102146288, -2.45182477425154, 0.596566274575251, 0.0];
Ap200 = [1.00, -4.875, 9.50625, -9.26859375, 4.518439453125, -0.881095693359375];
Bp200 = [0.862545460994275, -3.2832804496114, 4.67892032308828, -2.95798023879133, 0.699796870041299, 0.0];

oldMethod = 1; % Turn on or off the old method. The new one is Bastiaan's August 2008 updates
THReSHoLD = 2.0; % ADJUSTABLE threshold factor; 4.0 seems good
%%%%%%%%%%%%%%%%%%%
% use log domain (showed improved performance)
xxf = sqrt(real(xf.*conj(xf))+1e-20);
yyf = sqrt(real(yf.*conj(yf))+1e-20);
delayStruct.sxAll2(:,i) = 20*log10(xxf);
delayStruct.syAll2(:,i) = 20*log10(yyf);

mD = min(i-1,delayStruct.maxDelayb);
if oldMethod
    factor = 1.0;
    histLenb = 250;
    xthreshold = factor*median(delayStruct.sxAll2(:,i-mD:i),2);
    ythreshold = factor*median(delayStruct.syAll2(:,i-mD:i),2);
else
    xthreshold = sum(delayStruct.sxAll2(:,i-mD:i),2)/(delayStruct.maxDelayb+1);
    
    [yout, delayStruct.z200] = filter(Bp200, Ap200, delayStruct.syAll2(:,i), delayStruct.z200, 2);
    yout = yout/(delayStruct.maxDelayb+1);
    ythreshold = mean(delayStruct.syAll2(:,i-mD:i),2);
    ythreshold = yout;
end

delayStruct.bxspectrum(i) = getBspectrum(delayStruct.sxAll2(:,i), xthreshold, delayStruct.bandfirst, delayStruct.bandlast);
delayStruct.byspectrum(i) = getBspectrum(delayStruct.syAll2(:,i), ythreshold, delayStruct.bandfirst, delayStruct.bandlast);

delayStruct.bxhist(end-mD:end) = delayStruct.bxspectrum(i-mD:i);

delayStruct.bcount(:,i) = hisser2(delayStruct.byspectrum(i), flipud(delayStruct.bxhist), delayStruct.bandfirst, delayStruct.bandlast);
[delayStruct.fout(:,i), delayStruct.z500] = filter(Bp500, Ap500, delayStruct.bcount(:,i), delayStruct.z500, 2);
if oldMethod
    %delayStruct.new(:,i) = sum(delayStruct.bcount(:,max(1,i-histLenb+1):i),2); % using the history range
    tmpVec = [delayStruct.fout(1,i)*ones(2,1); delayStruct.fout(:,i); delayStruct.fout(end,i)*ones(2,1)]; % using the history range
    tmpVec = filter(ones(1,5), 1, tmpVec);
    delayStruct.new(:,i) = tmpVec(5:end);
    %delayStruct.new(:,i) = delayStruct.fout(:,i); % using the history range
else
    [delayStruct.fout(:,i), delayStruct.z500] = filter(Bp500, Ap500, delayStruct.bcount(:,i), delayStruct.z500, 2);
    % NEW CODE
    delayStruct.new(:,i) = filter([-1,-2,1,4,1,-2,-1], 1, delayStruct.fout(:,i)); %remv smth component
    delayStruct.new(1:end-3,i) = delayStruct.new(1+3:end,i);
    delayStruct.new(1:6,i) = 0.0;
    delayStruct.new(end-6:end,i) = 0.0;  % ends are no good
end
[valuen, tempdelay] = min(delayStruct.new(:,i));  % find minimum
if oldMethod
    threshold = valuen + (max(delayStruct.new(:,i)) - valuen)/4;
    thIndex = find(delayStruct.new(:,i) <= threshold);
    if (i > 1)
        delayDiff = abs(delayStruct.delay(i-1)-tempdelay+1);
        if (delayStruct.oneGoodEstimate & (max(diff(thIndex)) > 1) & (delayDiff < 10))
            % We consider this minimum to be significant, hence update the delay
            delayStruct.delay(i) = tempdelay;
        elseif (~delayStruct.oneGoodEstimate & (max(diff(thIndex)) > 1))
            delayStruct.delay(i) = tempdelay;
            if (i > histLenb)
                delayStruct.oneGoodEstimate = 1;
            end
        else
            delayStruct.delay(i) = delayStruct.delay(i-1);
        end
    else
        delayStruct.delay(i) = tempdelay;
    end
else
    threshold = THReSHoLD*std(delayStruct.new(:,i));   % set updata threshold 
    if ((-valuen > threshold) | (i < delayStruct.smlength)) % see if you want to update delay
        delayStruct.delay(i) = tempdelay;
    else
        delayStruct.delay(i) = delayStruct.delay(i-1);
    end
    % END NEW CODE
end
delayStructNew = delayStruct;

% administrative and plotting stuff
if( 0)
    figure(10);
    plot([1:length(delayStructNew.new(:,i))],delayStructNew.new(:,i),trueDelay*[1 1],[min(delayStructNew.new(:,i)),max(delayStructNew.new(:,i))],'r',[1 length(delayStructNew.new(:,i))],threshold*[1 1],'r:', 'LineWidth',2);
    %plot([1:length(delayStructNew.bcount(:,i))],delayStructNew.bcount(:,i),trueDelay*[1 1],[min(delayStructNew.bcount(:,i)),max(delayStructNew.bcount(:,i))],'r','LineWidth',2);
    %plot([thedelay,thedelay],[min(fcount(:,i)),max(fcount(:,i))],'r');
    %title(sprintf('bin count and known delay at time %5.1f s\n',(i-1)*(support/(fs*oversampling))));
    title(delayStructNew.oneGoodEstimate)
    xlabel('delay in frames');
    %hold off;
    drawnow
end
