function [B, A] = FiltreBoiteNoire(Freq, Gain, Fs, NZ, NP)

Nfft = 1024;

[~, I] = unique(Freq);
Freq = Freq(I);
Gain = Gain(I);

FreqQ = linspace(Fs/Nfft, Fs/2, Nfft/2);
GainQ = interp1(Freq, Gain, FreqQ, "spline");

FftA = [Gain(1) GainQ GainQ(Nfft/2-1:-1:1)];

Sdb = log(FftA);
c = ifft(Sdb);
Ns = Nfft / 2 + 1;
cf = [c(1), c(2:Ns-1)+c(Nfft:-1:Ns+1), c(Ns), zeros(1, Nfft-Ns)];
Cf = fft(cf);
Smp = 10 .^ (Cf/20);
Smpp = Smp(1:Ns);
fk = [0 FreqQ];
wt = 1 ./ (fk+1);
wk = 2*pi*fk/Fs;
[RB, RA] = invfreqz(Smpp, wk, NZ, NP, wt);

if (nargout == 0)
  Hh = freqz(RB, RA, Ns);
  semilogx(fk, db([Smpp(:), Hh(:)]));
  grid('on');
  xlabel('Frequency (Hz)');
  ylabel('Magnitude (dB)');
  title('Magnitude Frequency Response');
else
  B = RB;
  A = RA;
end
