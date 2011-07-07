function bspectrum=getBspectrum(ps,threshold,bandfirst,bandlast)
% function bspectrum=getBspectrum(ps,threshold,bandfirst,bandlast)
% compute binary spectrum using threshold spectrum as pivot
% bspectrum = binary spectrum (binary)
% ps=current power spectrum (float)
% threshold=threshold spectrum (float)
% bandfirst = first band considered
% bandlast = last band considered
  
% initialization stuff
  if( length(ps)<bandlast | bandlast>32 | length(ps)~=length(threshold)) 
  error('BinDelayEst:spectrum:invalid','Dimensionality error');
end

% get current binary spectrum
diff = ps - threshold;
bspectrum=uint32(0);
for(i=bandfirst:bandlast)
  if( diff(i)>0 ) 
    bspectrum = bitset(bspectrum,i);
  end
end
