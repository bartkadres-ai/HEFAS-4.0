# Teoria Sterowania — przykładowy egzamin (pełne rozwiązanie)

To jest osobny materiał, niezależny od głównego projektu.

---

## Schemat bazowy (zad. 1 i 2)

![Schemat sprzężenia zwrotnego](./schemat_sprzezenie_zwrotne.png)

---

# Zadanie 1

## Dane

$$
\hat g(s)=\frac{1}{s(s+6)}, \qquad \hat c(s)=k_p
$$

Wymagania: stabilność, \(e_{p\%}\le 10\%\), \(p\%\le 5\%\), \(t_{2\%}\le 2s\), \(t_{0.9}\) minimalne.

## Krok 1 — transmitancja zastępcza

$$
\hat g_z(s)=\frac{\hat c(s)\hat g(s)}{1+\hat c(s)\hat g(s)}
=\frac{k_p\frac{1}{s(s+6)}}{1+k_p\frac{1}{s(s+6)}}
=\frac{k_p}{s^2+6s+k_p}
$$

## Krok 2 — stabilność

Brak skróceń w \(\hat c(s)\hat g(s)\).  
Równanie charakterystyczne:

$$
s^2+6s+k_p=0
$$

Dla II rzędu: współczynniki dodatnie \(\Rightarrow k_p>0\).

## Krok 3 — błąd położeniowy

$$
e_p=\left|1-\hat g_z(0)\right|
=\left|1-\frac{k_p}{k_p}\right|=0
$$

$$
e_{p\%}=0\%\le 10\%
$$

## Krok 4 — linia pierwiastkowa

Zera: brak, bieguny: \(p_1=0,\ p_2=-6\).  
Centroid:

$$
c=\frac{0+(-6)}{2}=-3
$$

Asymptoty: \(\pm 90^\circ\).  
Punkt rozwidlenia: \(s_0=-3\).

![Linia pierwiastkowa](./zad1_linia_pierwiastkowa.png)

## Krok 5 — warunki \(p\%\) i \(t_{2\%}\)

Z obszaru dopuszczalnego (rysunek):

$$
s_1=-2.25,\qquad s_2=-3+3j
$$

Wzmocnienia graniczne:

$$
k_{p1}=-s(s+6)\big|_{s=-2.25}=8.44
$$

$$
k_{p2}=-s(s+6)\big|_{s=-3+3j}=18
$$

Zatem:

$$
k_p\in[8.44,\ 18]
$$

## Krok 6 — minimalizacja \(t_{0.9}\)

Wybór punktu najbardziej oddalonego od zera w dopuszczalnym obszarze:

$$
k_p=18
$$

## Odpowiedź zad. 1

$$
\boxed{k_p=18}
$$

---

# Zadanie 2

## Dane

$$
\hat g(s)=\frac{10}{s(s+10)},\qquad \hat c(s)=k_p
$$

Wymagania: stabilność, \(e_{p\%}\le 10\%\), \(z_f\ge 45^\circ\), \(\omega_m\) maksymalne.

## Krok 1 — transmitancja układu otwartego

$$
\hat g_o(s)=\hat c(s)\hat g(s)=k_p\frac{10}{s(s+10)}
=\frac{k_p}{s\left(1+\frac{s}{10}\right)}
$$

## Krok 2 — błąd położeniowy

Układ typu I (biegun w 0):

$$
e_p=0,\qquad e_{p\%}=0\%\le 10\%
$$

## Krok 3 — charakterystyka częstotliwościowa

Po podstawieniu \(s=j\omega\):

$$
\hat g_o(j\omega)=\frac{k_p}{j\omega\left(1+0.1j\omega\right)}
$$

Moduł:

$$
\left|\hat g_o(j\omega)\right|=\frac{k_p}{\omega\sqrt{1+(0.1\omega)^2}}
$$

Faza:

$$
\varphi(\omega)=-90^\circ-\arctan(0.1\omega)
$$

## Krok 4 — wykres Bodego (uproszczony)

![Uproszczony wykres Bodego](./zad2_bode_uproszczony.png)

Dla \(k_p=1\):  
\(\omega_m=1\ \text{rad/s}\), \(z_f=90^\circ\).

## Krok 5 — maksymalizacja \(\omega_m\) przy \(z_f\ge45^\circ\)

Wybieramy graniczny punkt:

$$
\omega_m=10\ \text{rad/s}
$$

bo:

$$
\varphi(10)\approx -135^\circ \Rightarrow z_f=45^\circ
$$

## Krok 6 — wyznaczenie \(k_p\)

Potrzebny wzrost modułu o \(20\ \mathrm{dB}\):

$$
20\log k_p=20 \Rightarrow k_p=10
$$

## Odczyty końcowe

$$
\omega_m=10\ \text{rad/s},\quad
z_f=45^\circ,\quad
\omega_\varphi=100\ \text{rad/s},\quad
z_m=40\ \text{dB}
$$

Stabilność:

$$
z_f>0,\ z_m>0
$$

## Odpowiedź zad. 2

$$
\boxed{k_p=10}
$$

---

# Zadanie 3

## Dane

$$
y''(t)+y(t)=u(t)
$$

Schemat:

![Schemat stanu](./zad3_schemat_stan.png)

Wymagania: stabilność wewn., \(e_p=0\), \(p\%\le5\%\), \(t_{2\%}\le1s\).

## Krok 1 — transmitancja obiektu

Przy zerowych warunkach początkowych:

$$
s^2Y(s)+Y(s)=U(s)\Rightarrow \hat g(s)=\frac{Y}{U}=\frac{1}{s^2+1}
$$

Bieguny: \(p_{1,2}=\pm j\).

## Krok 2 — model stanu

$$
x_1=y,\qquad x_2=\dot y
$$

$$
\dot x_1=x_2,\qquad \dot x_2=-x_1+u,\qquad y=x_1
$$

$$
A=\begin{bmatrix}0&1\\-1&0\end{bmatrix},\ 
b=\begin{bmatrix}0\\1\end{bmatrix},\ 
c=\begin{bmatrix}1&0\end{bmatrix},\ 
d=0
$$

## Krok 3 — sterowalność i obserwowalność

$$
W=[b\ Ab]=\begin{bmatrix}0&1\\1&0\end{bmatrix},\quad \det W=-1\neq0
$$

$$
V=\begin{bmatrix}c\\cA\end{bmatrix}
=\begin{bmatrix}1&0\\0&1\end{bmatrix},\quad \det V=1\neq0
$$

## Krok 4 — dobór biegunów zamkniętych

Przyjmujemy:

$$
\bar p_1=-5,\qquad \bar p_2=-10
$$

Docelowy mianownik:

$$
(s+5)(s+10)=s^2+15s+50
$$

## Krok 5 — dobór \(k\)

Sterowanie:

$$
u(t)=-kx(t)+k_fr(t),\qquad k=[k_1\ k_2]
$$

$$
\det(sI-A+bk)=s^2+k_2s+1+k_1
$$

Porównanie współczynników:

$$
k_2=15,\qquad 1+k_1=50\Rightarrow k_1=49
$$

$$
k=[49\ 15]
$$

## Krok 6 — dobór \(k_f\)

Warunek \(e_p=0\Rightarrow \hat g_k(0)=1\):

$$
\hat g_k(s)=\frac{k_f}{s^2+15s+50}
$$

$$
\hat g_k(0)=\frac{k_f}{50}=1\Rightarrow k_f=50
$$

## Krok 7 — wynik

$$
\hat g_k(s)=\frac{50}{(s+5)(s+10)}
$$

Wartości własne \(A-bk\): \(-5,\ -10\) \(\Rightarrow\) stabilny wewnętrznie.

## Odpowiedź zad. 3

$$
\boxed{k=[49\ 15],\quad k_f=50,\quad
\hat g_k(s)=\frac{50}{(s+5)(s+10)}}
$$

---

# Zadanie 4

## Dane

$$
y''(t)=u(t)
$$

Schemat:

![Schemat z obserwatorem](./zad4_schemat_obserwator.png)

Wymagania: stabilność wewn., \(e_p=0\), \(p\%\le5\%\), \(t_{2\%}\le1s\).

## Krok 1 — transmitancja i model obiektu

$$
s^2Y(s)=U(s)\Rightarrow \hat g(s)=\frac{1}{s^2}
$$

$$
A=\begin{bmatrix}0&1\\0&0\end{bmatrix},\ 
b=\begin{bmatrix}0\\1\end{bmatrix},\ 
c=\begin{bmatrix}1&0\end{bmatrix},\ d=0
$$

## Krok 2 — dobór \(k\) (gdy stan dostępny)

Zakładamy:

$$
\bar p_1=\bar p_2=-10
$$

Docelowy mianownik:

$$
(s+10)^2=s^2+20s+100
$$

$$
\det(sI-A+bk)=s^2+k_2s+k_1
$$

Porównanie:

$$
k_2=20,\qquad k_1=100
$$

$$
k=[100\ 20]
$$

## Krok 3 — dobór \(k_f\)

Warunek:

$$
e_p=0\Rightarrow \hat g_k(0)=1
$$

$$
\hat g_k(s)=\frac{k_f}{s^2+20s+100}
$$

$$
\frac{k_f}{100}=1\Rightarrow k_f=100
$$

## Krok 4 — projekt obserwatora

Obserwator:

$$
\dot{\tilde x}=A\tilde x+bu+l(y-\tilde y),\qquad \tilde y=c\tilde x
$$

Przyjmujemy:

$$
l=\begin{bmatrix}l_1\\l_2\end{bmatrix}
$$

Wybór biegunów obserwatora:

$$
\mu_1=\mu_2=-5
$$

$$
\det(\mu I-A+lc)=\mu^2+l_1\mu+l_2=(\mu+5)^2=\mu^2+10\mu+25
$$

Stąd:

$$
l_1=10,\qquad l_2=25
$$

$$
l=\begin{bmatrix}10\\25\end{bmatrix}
$$

## Krok 5 — wynik końcowy

Sterowanie:

$$
u(t)=-k\tilde x(t)+k_fr(t)
$$

czyli:

$$
k=[100\ 20],\quad k_f=100,\quad
l=\begin{bmatrix}10\\25\end{bmatrix}
$$

Transmitancja zastępcza (zasada separacji):

$$
\hat g_z(s)=\frac{100}{(s+10)^2}
$$

Wartości własne układu rozszerzonego:

$$
\{-10,-10,-5,-5\}
$$

\(\Rightarrow\) stabilny wewnętrznie.

## Odpowiedź zad. 4

$$
\boxed{
k=[100\ 20],\ 
k_f=100,\ 
l=\begin{bmatrix}10\\25\end{bmatrix},\ 
\hat g_z(s)=\frac{100}{(s+10)^2}
}
$$

---

# Odpowiedzi końcowe (zbiorczo)

$$
\boxed{
\text{Zad.1: }k_p=18,\quad
\text{Zad.2: }k_p=10,\quad
\text{Zad.3: }k=[49\ 15],\ k_f=50,\quad
\text{Zad.4: }k=[100\ 20],\ k_f=100,\ l=\begin{bmatrix}10\\25\end{bmatrix}
}
$$
