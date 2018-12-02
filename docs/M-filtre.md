# Recréer un filtre à partir d'une réponse en gain

**Note :** ce processus a fonctionné sous MATLAB mais pas sous GNU Octave.
La fonction `invfreqz` présente dans le paquet `signal` de Octave ne fonctionne pas correctement.

On pose la fréquence d'échantillonnage.

```
Fs = 44100;
```

On pose la taille de FFT désirée.

```
Nfft = 1024;
```

On importe les deux premières colonnes du fichier profil : fréquence et phase.

```
cd mon-dossier;
Data = load("lo.dat");
Freq = Data(:, 1);
Gain = Data(:, 2);
```

On élimine les doublons éventuels.

```
[~, I] = unique(Freq);
Freq = Freq(I);
Gain = Gain(I);
```

On calcule les points à interpoler pour reconstruire la partie amplitude de la DFT.
On saute le point 0 qui correspond à la composante DC.

```
FreqQ = linspace(Fs/Nfft, Fs/2, Nfft/2);
```

On interpole les valeurs de gain correspondantes.

```
GainQ = interp1(Freq, Gain, FreqQ, "spline");
```

On met bout à bout les éléments de la DFT :
- la composante DC
- les fréquences positives
- les fréquences négatives

```
FftA = [Gain(1) GainQ GainQ(Nfft/2-1:-1:1)];
```

On calcule la *phase minimum*.
On pose également `NP` et `NZ` afin de définir l'ordre du filtre souhaité.

Référence : https://ccrma.stanford.edu/~jos/pasp/Converting_Desired_Amplitude_Response.html

```
NP = 7;
NZ = 7;
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
[B, A] = invfreqz(Smpp, wk, NZ, NP, wt);
```

On affiche un comparatif de la réponse des 2 filtres :

```
Hh = freqz(B, A, Ns);
semilogx(fk, db([Smpp(:), Hh(:)]));
grid('on');
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
title('Magnitude Frequency Response');
```
