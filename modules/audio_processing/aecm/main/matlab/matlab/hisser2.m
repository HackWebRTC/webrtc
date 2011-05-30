function  bcount=hisser2(bs,bsr,bandfirst,bandlast)
% function  bcount=hisser(bspectrum,bandfirst,bandlast)
% histogram for the binary spectra
% bcount= array of bit counts 
% bs=binary spectrum (one int32 number each)  
% bsr=reference binary spectra (one int32 number each)
% blockSize = histogram over blocksize blocks
% bandfirst = first band considered
% bandlast = last band considered

% weight all delays equally
maxDelay = length(bsr);

% compute counts (two methods; the first works better and is operational)
bcount=zeros(maxDelay,1);
for(i=1:maxDelay)
 % the delay should have low count for low-near&high-far and high-near&low-far
 bcount(i)= sum(bitget(bitxor(bs,bsr(i)),bandfirst:bandlast));  
 % the delay should have low count for low-near&high-far (works less well)
% bcount(i)= sum(bitget(bitand(bsr(i),bitxor(bs,bsr(i))),bandfirst:bandlast));
end
