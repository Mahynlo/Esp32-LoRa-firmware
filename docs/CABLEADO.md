# Cableado ESP32 + LoRa DX-LR30 + Relé HL-52S

Hardware de los dos equipos: el **gateway** (ESP32 + LoRa, con Wi-Fi) y el **nodo** del motor (ESP32 + LoRa + relé): modelo de cada placa, pines usados, colores de cable por conexión, alimentación, configuración de software y verificaciones.

> **Resumen**
> - **MCU:** ESP32 DevKit de 30 pines (módulo ESP32-WROOM-32) sobre un shield de expansión con jack DC.
> - **Radio:** DX-LR30-900M22S (Semtech SX1262) en adaptador **DX-PJ27-V1.2**, por **SPI**.
> - **Relés:** módulo **HL-52S V1.0** de 2 canales, activo en bajo, con optoacopladores.
> - **Alimentación del LoRa:** 3.3 V (medido: **3.27 V** en el pin VCC del shield, jumper JUMP en 3.3V).
> - **Cables:** dupont hembra–hembra en tiras; colores definidos abajo. **Rojo = 3.3 V, negro = GND, naranja con etiqueta "5V" = 5 V del relé.**

---

## 1. Hardware

### 1.1 ESP32 DevKit (30 pines)

| Característica | Detalle |
|---|---|
| Módulo | ESP32-WROOM-32 (marcado "ESP-32" en el blindaje metálico) |
| CPU | Xtensa LX6 doble núcleo, 240 MHz |
| Flash | 4 MB (típico en esta placa) |
| Conectividad | Wi-Fi 2.4 GHz + Bluetooth |
| USB | USB-C con conversor USB-serial (CH340) |
| Botones | **EN** (reset) y **BOOT** |
| Lógica | **3.3 V** (los GPIO no toleran 5 V) |
| Pines | 2 tiras de 15 |

Orden físico de las tiras (como en la serigrafía de la placa):

```
Tira superior:  VIN  GND  D13  D12  D14  D27  D26  D25  D33  D32  D35  D34  VN   VP   EN
Tira inferior:  3V3  GND  D15  D2   D4   D16  D17  D5   D18  D19  D21  RX0  TX0  D22  D23
```

Notas de los GPIO relevantes:
- **GPIO 34, 35, 36 (VP) y 39 (VN):** solo entrada, sin pull-up/pull-down internos.
- **GPIO 0, 2, 5, 12, 15:** pines de arranque (strapping). **GPIO 5** se usa como NSS del LoRa; funciona sin problema porque en reposo queda en alto, igual que su estado de arranque.
- **GPIO 1 (TX0) y 3 (RX0):** serial USB (monitor y programación); no usar.

### 1.2 Shield de expansión ESP32 (30 pines)

| Elemento | Detalle |
|---|---|
| Entrada DC | Jack **DC 6.5–16 V** |
| USB de alimentación | USB-C y micro-USB (USB5V) |
| Jumper **JUMP** | Selecciona el voltaje de los pines **V/VCC** del shield: **5V** o **3.3V**. **Debe estar en 3.3V.** |
| Filas por GPIO | Cada GPIO tiene **3 pines: S (señal), V (voltaje) y G (tierra)**. Las señales van **solo en S**. |
| Bloque de alimentación | 4× GND, 2× 3.3V, 2× 5V |
| Bloque auxiliar | GND, VCC, D21, D22 (2 filas) |
| LED | PWR-LED |

### 1.3 Módulo LoRa

| Característica | Detalle |
|---|---|
| Módulo | **DX-LR30-900M22S** (DX-SMART) |
| Chip | **Semtech SX1262** |
| Banda | 850–930 MHz (usamos **915 MHz**, banda ISM 902–928 MHz de México) |
| Potencia TX | hasta +22 dBm |
| Interfaz | **SPI** + BUSY, NRST, DIO1 y switch de RF (TXEN/RXEN) |
| Oscilador | **Cristal, no TCXO** → en RadioLib `tcxoVoltage = 0.0` |
| Alimentación | **3.3 V** (máx. 3.7 V; nunca 5 V) |
| Adaptador | **DX-PJ27-V1.2**: conector macho 2×6 y conector SMA para antena |

Conector del DX-PJ27, visto con la serigrafía de frente y el conector a la derecha (▶ = fila de alimentación):

```
            Columna izq.   Columna der.
   Fila 1 ▶   VCC            GND
   Fila 2     NSS            NRST
   Fila 3     MOSI           SCK
   Fila 4     DIO1           MISO
   Fila 5     DIO2           BUSY
   Fila 6     RXEN           TXEN
```

### 1.4 Módulo de relés

| Característica | Detalle |
|---|---|
| Módulo | **HL-52S V1.0**, 2 canales |
| Relés | Songle **SRD-05VDC-SL-C** (bobina 5 V; contactos 10 A @ 250 VCA / 30 VCD) |
| Disparo | **Activo en bajo** (LOW = relé encendido) |
| Aislamiento | Optoacopladores; jumper **JD-VCC ↔ VCC** |
| Header de control | `Gnd  In1  In2  Vcc` |
| Bloque de alimentación | `JD-VCC  VCC  GND` (con jumper amarillo de fábrica) |
| Bornes de potencia | NO / COM / NC por relé |

---

## 2. Asignación de pines (ESP32)

| Señal | GPIO | Pin en placa | Dirección | Notas |
|---|---|---|---|---|
| LoRa SCK | 18 | D18 | Salida | SPI (VSPI) |
| LoRa MISO | 19 | D19 | Entrada | SPI (VSPI) |
| LoRa MOSI | 23 | D23 | Salida | SPI (VSPI) |
| LoRa NSS | 5 | D5 | Salida | Chip select |
| LoRa NRST | 27 | D27 | Salida | Reset del SX1262 |
| LoRa BUSY | 26 | D26 | Entrada | Estado del SX1262 |
| LoRa DIO1 | 33 | D33 | Entrada | Interrupción (TX listo / RX listo / timeout) |
| LoRa TXEN | 25 | D25 | Salida | Switch de RF (manejado por RadioLib) |
| LoRa RXEN | 32 | D32 | Salida | Switch de RF (manejado por RadioLib) |
| Relé In1 | 16 | D16 | Salida | Activo en bajo |
| Relé In2 | 17 | D17 | Salida | Activo en bajo |
| LoRa DIO2 | — | — | — | No se conecta |

Libres para uso futuro: D13, D12, D14, D15, D2, D4, D21, D22 y las entradas D34, D35, VP, VN.

---

## 3. Cableado con colores

Regla general: **rojo = 3.3 V**, **negro = GND**, **naranja suelto con etiqueta "5V" = 5 V** (solo al relé). El naranja de la tira del LoRa es la señal NSS, **no** 5 V.

### 3.1 Módulo LoRa → ESP32 (11 cables + DIO2 libre)

**Tira izquierda** (rojo · naranja · amarillo · verde · azul · morado, sin separar):

| Fila | Pin DX-PJ27 | Color | Pin ESP32 | Fila del shield |
|---|---|---|---|---|
| 1 ▶ | VCC | 🔴 Rojo | VCC (3.3 V) | Bloque GND/VCC/D21/D22 |
| 2 | NSS | 🟠 Naranja | D5 | S |
| 3 | MOSI | 🟡 Amarillo | D23 | S |
| 4 | DIO1 | 🟢 Verde | D33 | S |
| 5 | DIO2 | 🔵 Azul | **sin conectar** (punta con cinta) | — |
| 6 | RXEN | 🟣 Morado | D32 | S |

**Tira derecha** (negro · blanco · gris · morado · azul · verde, sin separar):

| Fila | Pin DX-PJ27 | Color | Pin ESP32 | Fila del shield |
|---|---|---|---|---|
| 1 ▶ | GND | ⚫ Negro | GND | Bloque GND/VCC/D21/D22 |
| 2 | NRST | ⚪ Blanco | D27 | S |
| 3 | SCK | 🩶 Gris | D18 | S |
| 4 | MISO | 🟣 Morado | D19 | S |
| 5 | BUSY | 🔵 Azul | D26 | S |
| 6 | TXEN | 🟢 Verde | D25 | S |

### 3.2 Cómo quedan las tiras del ESP32

```
Tira superior:  VIN  GND  D13  D12  D14  D27     D26   D25    D33    D32     D35  D34  VN  VP  EN
                 -    -    -    -    -   BLANCO  AZUL  VERDE  VERDE  MORADO   -    -   -   -   -
                                         NRST    BUSY  TXEN   DIO1   RXEN

Tira inferior:  3V3  GND  D15  D2   D4   D16     D17     D5       D18   D19     D21  RX0  TX0  D22  D23
                 -    -    -    -    -   (relé)  (relé)  NARANJA  GRIS  MORADO   -    -    -    -   AMARILLO
                                         In1     In2     NSS      SCK   MISO                        MOSI
```

En la tira superior quedan **seguidos morado, verde, verde, azul y blanco** (D32 → D27). En la inferior, **naranja, gris y morado seguidos** (D5, D18, D19), cuatro pines libres y el **amarillo en el último** (D23).

### 3.3 Relé HL-52S → ESP32 (6 cables) — solo en el nodo

**Quitar el jumper amarillo JD-VCC ↔ VCC** antes de conectar.

| Pin del relé | Color | Conectar a | Notas |
|---|---|---|---|
| Gnd (header) | ⚫ Negro (suelto) | GND | |
| In1 | 🩶 Gris | D16 | Par gris–blanco unido |
| In2 | ⚪ Blanco | D17 | Par gris–blanco unido |
| Vcc (header) | 🔴 Rojo (suelto) | 3.3 V | Alimenta los optoacopladores a 3.3 V |
| JD-VCC | 🟠 Naranja (suelto, etiqueta **"5V"**) | 5 V (pin 5V del bloque de alimentación o VIN) | Alimenta las bobinas |
| VCC (bloque) | — | sin conectar | |
| GND (bloque) | ⚫ Negro (suelto) | GND | |

Por qué así: con el jumper puesto y todo a 5 V, el HIGH de 3.3 V del ESP32 no apaga del todo el optoacoplador y el relé puede quedarse pegado. Separando las alimentaciones, la lógica queda a 3.3 V y las bobinas a 5 V.

---

## 4. Alimentación

| Punto | Valor | Estado |
|---|---|---|
| VCC del shield (jumper JUMP en 3.3V) | **3.27 V medidos** | ✅ correcto para el LoRa |
| Pines 3.3V del bloque de alimentación | 3.3 V | alternativa para los rojos |
| Pines 5V / VIN | 5 V | solo para JD-VCC del relé |

Recomendaciones:
- Para uso final, alimentar por el **jack DC (6.5–16 V)**: ESP32 + 2 relés + LoRa transmitiendo a 22 dBm superan lo cómodo para el USB de una PC.
- Opcional: capacitor de **100–470 µF** entre VCC y GND junto al módulo LoRa, para los picos de corriente al transmitir.

---

## 5. Plan de corte de la tira dupont

La tira de 40 cables repite: **café, rojo, naranja, amarillo, verde, azul, morado, gris, blanco, negro**.

| Tramo | Colores | Uso |
|---|---|---|
| 6 | negro, blanco, gris, morado, azul, verde | LoRa 1 · tira derecha |
| 1 | café | repuesto |
| 6 | rojo, naranja, amarillo, verde, azul, morado | LoRa 1 · tira izquierda |
| 7 | gris, blanco, negro, café, rojo, naranja, amarillo | relé (gris–blanco unidos; negro, rojo, naranja sueltos) |
| 6 | verde, azul, morado, gris, blanco, negro | LoRa 2 · tira derecha |
| 1 | café | repuesto |
| 6 | rojo, naranja, amarillo, verde, azul, morado | LoRa 2 · tira izquierda |
| 7 | gris, blanco, negro, café, rojo, naranja, amarillo | sobrante (segundo negro para GND del bloque del relé) |

Los dos equipos LoRa usan **exactamente los mismos colores en los mismos pines**.

---

## 6. Software

Los pines están en `include/pins.h` y los parámetros de radio en `include/lora_config.h` (iguales en los dos firmwares). El protocolo y la seguridad del enlace: [LORA.md](LORA.md).

Dos detalles del hardware que el firmware ya respeta:

- **Radio:** 915 MHz, BW 125 kHz, SF9, CR 4/7, sync word privado, preámbulo 8 y `tcxoVoltage = 0.0` (cristal). Para comunicarse, los dos equipos deben compartir frecuencia, BW, SF, CR y sync word.
- **Relé sin "clic" al arrancar:** el pin del relé arranca como entrada (alta impedancia = apagado). Para encender se pone en LOW; para apagar se vuelve a entrada, así el relé se suelta por completo tanto con su Vcc a 3,3 V (cableado de la sección 3.3) como a 5 V.

## 7. Verificación

### 7.1 Antes de encender

- [ ] Jumper **JUMP en 3.3V**; VCC medido ≈ 3.3 V (**3.27 V** ✅).
- [ ] Rojo en **VCC** y negro en **GND** (no invertidos).
- [ ] Todas las señales en la **fila S** del shield (continuidad entre el pin usado y la patita del ESP32 con el mismo nombre).
- [ ] Tiras del módulo con **rojo/negro en la fila ▶**, sin pines libres a los lados.
- [ ] Azul de **DIO2** sin conectar, con cinta en la punta.
- [ ] **Antena conectada** antes de cualquier programa que transmita.
- [ ] (Relé) Jumper amarillo quitado; naranja "5V" solo en JD-VCC.

### 7.2 Al encender

Si algo se calienta, huele a quemado, el puerto USB se conecta y desconecta solo, o el PWR-LED se comporta raro: **desconectar de inmediato** y revisar la alimentación.

### 7.3 Prueba de diagnóstico (no transmite)

Con el programa de diagnóstico del proyecto de pruebas (`esp32_lora_prueba`). Resultado esperado en el monitor (115200, presionar EN):

```
[OK] BUSY (D26) y NRST (D27)
Sync word: 0x14 0x24
[OK] SPI: naranja D5, gris D18, morado D19, amarillo D23
[OK] DIO1 (verde) esta en GPIO 33
```

| Resultado | Qué revisar |
|---|---|
| BUSY no baja | Azul en D26, blanco en D27, alimentación |
| Sync word `0x00 0x00` | MISO (morado D19), SCK (gris D18), VCC |
| Sync word `0xFF 0xFF` | NSS (naranja D5), MOSI (amarillo D23) |
| DIO1 en otro GPIO | Mover el verde de la tira izquierda a D33 |

TXEN y RXEN se confirman con el enlace: si están mal, la radio inicia pero el alcance es casi nulo. Con los dos firmwares cargados, el monitor del gateway muestra `[lora] Nodo del motor conectado` y la app la señal LoRa.

---

## 8. Seguridad del lado de potencia (relés)

- Cada relé: **COM + NO** para encender la carga cuando el relé se activa.
- Para motores medianos o de 127/220 VCA, usar el relé para activar un **contactor**; la corriente de arranque de un motor supera fácilmente los 10 A del relé.
- Trabajar la parte de CA **siempre desconectada** y mantener esos cables separados de la parte de bajo voltaje dentro de la caja.
- Primeras pruebas: **sin carga** en los bornes, solo verificar el LED y el clic de cada relé.

---

## 9. Equipos

| Equipo | Contenido | Firmware |
|---|---|---|
| **Nodo** (en la caja, junto al motor) | ESP32 + shield + LR30 + relé HL-52S | `pio run -e nodo -t upload` |
| **Gateway** (donde hay Wi-Fi) | ESP32 + shield + LR30 | `pio run -e gateway -t upload` |
| Nodo STM32 | DX-PJ26 V1.2 + LR30 | Proyecto `stm32_lora` (pruebas de radio; no habla el protocolo cifrado) |
