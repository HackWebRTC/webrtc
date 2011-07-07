speakerType = 'fm';
%for k=2:5
%for k=[2 4 5]
for k=3
    scenario = int2str(k);
    fprintf('Current scenario: %d\n',k)
    mainProgram
    %saveFile = [speakerType, '_s_',scenario,'_delayEst_v2_vad_man.wav'];
    %wavwrite(emic,fs,nbits,saveFile);
    %saveFile = ['P:\Engineering_share\BjornV\AECM\',speakerType, '_s_',scenario,'_delayEst_v2_vad_man.pcm'];
    %saveFile = [speakerType, '_s_',scenario,'_adaptMu_adaptGamma_withVar_gammFilt_HSt.pcm'];
    saveFile = ['scenario_',scenario,'_090417_backupH_nlp.pcm'];
    fid=fopen(saveFile,'w');fwrite(fid,int16(emicrophone),'short');fclose(fid);
    %pause
end
