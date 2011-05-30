% Outputs a file for testing purposes. 
%
% Adjust the following parameters to suit. Their purpose becomes more clear on
% viewing the gain plots.
% MaxGain: Max gain in dB
% MinGain: Min gain at overload (0 dBov) in dB
% CompRatio: Compression ratio, essentially determines the slope of the gain
%            function between the max and min gains
% Knee: The smoothness of the transition to max gain (smaller is smoother)
MaxGain = 5; MinGain = 0; CompRatio = 3; Knee = 1;

% Compute gains
zeros = 0:31; lvl = 2.^(1-zeros); 
A = -10*log10(lvl) * (CompRatio - 1) / CompRatio;
B = MaxGain - MinGain;
gains = round(2^16*10.^(0.05 * (MinGain + B * ( log(exp(-Knee*A)+exp(-Knee*B)) - log(1+exp(-Knee*B)) ) / log(1/(1+exp(Knee*B))))));
fprintf(1, '\t%i, %i, %i, %i,\n', gains);

% Save gains to file
fid = fopen('gains', 'wb');
if fid == -1
	error(sprintf('Unable to open file %s', filename));
	return
end
fwrite(fid, gains, 'int32');
fclose(fid);

% Plotting
in = 10*log10(lvl); out = 20*log10(gains/65536);
subplot(121); plot(in, out); axis([-60, 0, -5, 30]); grid on; xlabel('Input (dB)'); ylabel('Gain (dB)');
subplot(122); plot(in, in+out); axis([-60, 0, -60, 10]); grid on; xlabel('Input (dB)'); ylabel('Output (dB)');
zoom on;
