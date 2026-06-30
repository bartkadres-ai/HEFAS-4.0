# Teoria Sterowania — Zadanie 2

To jest **osobny plik README**, niepowiązany z głównym projektem.

Poniżej jest pełne rozwiązanie **Zadania 2** krok po kroku, w stylu egzaminowym.

---

## Treść zadania

Dany jest obiekt opisany transmitancją:

$$
\hat{g}(s) = \frac{10}{s(s+10)}
$$

Należy, wykorzystując **metodę częstotliwościową**, zaprojektować układ sterowania z jednostkowym sprzężeniem zwrotnym, który spełnia wymagania:

1. stabilność,
2. błąd położeniowy $e_{p\%} \le 10\%$,
3. zapas fazy $z_f \ge 45^\circ$,
4. modułowa pulsacja przejścia $\omega_m$ możliwie największa.

Wskazówka: użyć możliwie najprostszego regulatora, czyli regulatora proporcjonalnego.

---

## Krok 1 — wybór regulatora

Wybieramy regulator proporcjonalny:

$$
\hat{c}(s) = k_p
$$

Układ otwarty ma więc transmitancję:

$$
\hat{g}_o(s) = \hat{c}(s)\hat{g}(s) = k_p \cdot \frac{10}{s(s+10)}
$$

Upraszczamy:

$$
\hat{g}_o(s) = \frac{10k_p}{s(s+10)} = \frac{k_p}{s\left(1 + \frac{s}{10}\right)}
$$

---

## Krok 2 — sprawdzenie błędu położeniowego

Mamy biegun w zerze, więc układ jest **typu I**.

To oznacza, że dla skoku jednostkowego:

$$
e_p = 0
$$

czyli:

$$
e_{p\%} = 0\% \le 10\%
$$

### Wniosek

Warunek błędu położeniowego jest spełniony automatycznie.

---

## Krok 3 — charakterystyka częstotliwościowa układu otwartego

Podstawiamy:

$$
s = j\omega
$$

Otrzymujemy:

$$
\hat{g}_o(j\omega) = \frac{k_p}{j\omega\left(1 + 0.1j\omega\right)}
$$

---

## Krok 4 — moduł i faza

### Moduł

$$
\left|\hat{g}_o(j\omega)\right| = \frac{k_p}{\omega\sqrt{1 + (0.1\omega)^2}}
$$

Po przejściu do decybeli:

$$
20\log\left|\hat{g}_o(j\omega)\right|
= 20\log k_p - 20\log\omega - 20\log\sqrt{1+0.01\omega^2}
$$

To są trzy składniki:

1. $20\log k_p$ — przesunięcie wykresu w górę lub w dół,
2. $-20\log\omega$ — całkowanie,
3. $-20\log\sqrt{1+0.01\omega^2}$ — biegun przy $\omega = 10$.

---

### Faza

$$
\varphi(\omega) = -90^\circ - \arctan(0.1\omega)
$$

Czyli:

- całka daje stale $-90^\circ$,
- biegun przy $10$ rad/s dokłada dodatkowe opóźnienie fazowe.

---

## Krok 5 — przypadek bazowy: $k_p = 1$

Najpierw analizujemy układ dla:

$$
k_p = 1
$$

Wtedy wykres modułu nie jest przesunięty.

Z uproszczonych charakterystyk logarytmicznych odczytujemy:

$$
\omega_m = 1 \text{ rad/s}
$$

Dla tej pulsacji faza wynosi około:

$$
\varphi(1) \approx -90^\circ
$$

więc zapas fazy:

$$
z_f = 180^\circ - 90^\circ = 90^\circ
$$

### Wniosek

Dla $k_p = 1$ układ ma bardzo duży zapas fazy, więc można próbować zwiększyć $k_p$.

---

## Krok 6 — maksymalizacja modułowej pulsacji przejścia

Chcemy, żeby:

$$
\omega_m
$$

była możliwie największa.

Zwiększanie $k_p$ podnosi wykres modułu o:

$$
20\log k_p
$$

ale jednocześnie zmniejsza zapas fazy.

Musimy więc dobrać takie $k_p$, żeby punkt przecięcia z osią 0 dB wypadł dokładnie tam, gdzie zapas fazy jest jeszcze równy $45^\circ$.

Z wykresu uproszczonego wiadomo, że taka sytuacja wypada dla:

$$
\omega_m = 10 \text{ rad/s}
$$

bo wtedy:

$$
\varphi(10) = -90^\circ - 45^\circ = -135^\circ
$$

a więc:

$$
z_f = 180^\circ - 135^\circ = 45^\circ
$$

Warunek na zapas fazy jest spełniony dokładnie na granicy.

---

## Krok 7 — wyznaczenie $k_p$

Skoro chcemy przesunąć punkt przecięcia z 0 dB z:

$$
\omega_m = 1
$$

na:

$$
\omega_m = 10
$$

to musimy podnieść wykres modułu o **20 dB**.

Zatem:

$$
20\log k_p = 20
$$

czyli:

$$
\log k_p = 1
$$

stąd:

$$
k_p = 10
$$

---

## Krok 8 — odczyt końcowych parametrów

Dla:

$$
k_p = 10
$$

otrzymujemy:

### Modułowa pulsacja przejścia

$$
\omega_m = 10 \text{ rad/s}
$$

### Zapas fazy

$$
z_f = 45^\circ
$$

### Fazowa pulsacja przejścia

Z wykresu uproszczonego:

$$
\omega_\varphi = 100 \text{ rad/s}
$$

### Zapas modułu

Przy tej pulsacji odczytujemy:

$$
z_m = 40 \text{ dB}
$$

---

## Krok 9 — wniosek o stabilności

Układ jest stabilny, bo:

$$
z_f > 0
\quad \text{oraz} \quad
z_m > 0
$$

czyli:

$$
45^\circ > 0
\quad \text{i} \quad
40\text{ dB} > 0
$$

---

## Odpowiedź końcowa

Dobieramy regulator:

$$
\hat{c}(s) = 10
$$

Czyli:

$$
k_p = 10
$$

Końcowe parametry układu:

$$
\omega_m = 10 \text{ rad/s}
$$

$$
z_f = 45^\circ
$$

$$
\omega_\varphi = 100 \text{ rad/s}
$$

$$
z_m = 40 \text{ dB}
$$

Układ spełnia warunki:

- stabilności,
- błędu położeniowego,
- zapasu fazy,
- maksymalizacji modułowej pulsacji przejścia.

---

## Krótka wersja „co pisać na egzaminie”

1. Przyjmuję regulator proporcjonalny: $c(s)=k_p$.
2. Wyznaczam transmitancję układu otwartego:

   $$
   g_o(s)=k_p\frac{10}{s(s+10)}
   $$

3. Stwierdzam, że układ jest typu I, więc:

   $$
   e_p=0
   $$

4. Wyznaczam charakterystykę częstotliwościową:

   $$
   g_o(j\omega)=\frac{k_p}{j\omega(1+0.1j\omega)}
   $$

5. Dla $k_p=1$ odczytuję:

   $$
   \omega_m=1,\quad z_f=90^\circ
   $$

6. Zwiększam $k_p$, żeby przesunąć przecięcie do:

   $$
   \omega_m=10
   $$

   gdzie:

   $$
   z_f=45^\circ
   $$

7. Warunek:

   $$
   20\log k_p=20
   $$

   daje:

   $$
   k_p=10
   $$

8. Ostatecznie:

   $$
   \omega_m=10,\quad z_f=45^\circ,\quad \omega_\varphi=100,\quad z_m=40\text{ dB}
   $$

---

Jeśli GitHub poprawnie renderuje matematykę, ten plik powinien już pokazać równania normalnie.
