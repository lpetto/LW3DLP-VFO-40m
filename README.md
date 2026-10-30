Manual de Construcción y Uso:
Panel de Control Digital para 40 Metros (AM/BLU)
Autor y Creador: Luis Ángel Petitto (LW3DLP)Licencia: Creative Commons BY-NC-SA 4.0Versión del Software: 1.0 (Octubre 2026)
1. Introducción
Este proyecto es un panel de control digital (VFO/BFO y controlador) diseñado específicamente para equipos de radioaficionados en la banda de 40 metros (7 MHz). Utiliza un microcontrolador ESP32-S3 NodeMCU, un sintetizador de frecuencias Si5351, y tres pantallas TFT de 1.44 pulgadas (128x128) que se unen visualmente para formar una pantalla gigante de información de 3x128 de ancho x 128 de alto.
El sistema permite sintonizar en AM (Amplitud Modulada) y BLU (Banda Lateral Única, inyectando una portadora de 455 kHz), guardar memorias, ajustar el clarificador (RIT sobre la frecuencia de portadora) y calibrar el cristal del Si5351 para que la frecuencia sea exacta al Hertz.
2. Lista de Materiales (Hardware)
Para construir este panel, necesitarás:
•
1x Microcontrolador ESP32-S3 NodeMCU (módulo de 38 pines).
•
3x Pantallas TFT ST7735 de 1.44" (128x128 píxeles).
•
1x Módulo generador de reloj Si5351.
•
1x Encoder rotativo incremental con pulsador (con pull-ups).
•
1x (Opcional) Módulo de relé para habilitar la transmisión.
•
1x Pulsador o pedal PTT (Push To Talk).
•
Resistencias, capacitores y cables para armar el circuito.
3. Diagrama de Conexiones (Pinout)
El proyecto no incluye una placa de circuito impreso (PCB). El diseño fue creciendo, el PCB original quedó obsoleto y se trabajó "modificándolo" con conexiones cableadas. No se creó un PCB final, pero no dudo que otro colega se encargará de compartirlo. A continuación, el detalle exacto de cada pin del ESP32-S3:
3.1 Pantallas TFT (Comparten el bus SPI)
Las tres pantallas se conectan en paralelo en los pines de datos, pero cada una debe tener su propio Chip Select (CS) y Reset (RST) para que el microcontrolador pueda controlarlas por separado.
Pines de datos compartidos (van a las 3 pantallas en paralelo):
•
MOSI (Datos): Pin 23
•
SCLK (Reloj): Pin 18
•
DC (Data/Command): Pin 4
Pines individuales (uno por cada pantalla):
•
TFT 1 (Izquierda – Mhz/kHz): Pin CS: 5 | Pin RST: 2
•
TFT 2 (Centro – Hz/Modos):Pin CS: 17 | Pin RST: 13
•
TFT 3 (Derecha – Medidores):Pin CS: 15 | Pin RST: 14
3.2 Encoder Rotativo y PTT
•
Encoder CLK (A): Pin 34 (Requiere pull-up externo de 10K a 3.3V)
•
Encoder DT (B): Pin 35 (Requiere pull-up externo de 10K a 3.3V)
•
Encoder SW (Pulsador): Pin 33
•
PTT (Pulsador de TX): Pin 26 (Activo en LOW / a masa).
•
Relé de TX (TX_ENABLE): Pin 25 (Se pone en HIGH - 3.3V - para transmitir).
⚠️ Nota importante sobre el relé: Como el ESP32-S3 entrega 3.3V en sus salidas, deberás usar un transistor para controlar el relé, si es optoacoplado mejor.
3.3 Sintetizador Si5351 (Bus I2C)
•
SDA: Pin 22 (Requiere pull-up de 4K7 a 3.3V)
•
SCL: Pin 21 (Requiere pull-up de 4K7 a 3.3V)
Nota importante: El Si5351 genera 3 señales. Para inyectar una señal limpia al receptor, es esencial colocar filtros pasa-bajos a la salida de cada CLK1 y 2:
•
CLK0: Señal de TX (Onda cuadrada).
•
CLK1: Oscilador Local de RX (Frecuencia + 455 kHz).
•
Filtro para CLK1 (RX VFO - 7.455 MHz a 7.755 MHz):
•
Topología: Pasa bajos π (pi) de 3er orden.
•
L1: 10 μH (Inductor axial 0410 o código "100").
•
C1, C2: 47 pF.
•
Frecuencia de corte aproximada: ~7.5 MHz (para dejar pasar la banda de 40m y atenuar armónicos).
•
CLK2: BFO / Clarificador (455 kHz).
•
Filtro para CLK2 (BFO - 455 kHz):
•
Topología: Pasa bajos π (pi) de 3er orden.
•
L2: 90 μH o 100 μH.
•
C1, C2: 1500 pF (1.5 nF).
•
Frecuencia de corte aproximada: ~455 kHz (para la señal de BFO).
Cálculo de la frecuencia de corte (referencia):Para un filtro π de 3er orden, la frecuencia de corte se puede estimar con la fórmula:
•
fc ≈ 1 / (2π * √(L * C))
•
Para CLK1: L = 10 μH, C = 47 pF → fc ≈ 7.34 MHz.
•
Para CLK2: L = 90 μH, C = 1500 pF → fc ≈ 433 kHz.Ambos valores son coherentes con las frecuencias de operación (7.5 MHz y 455 kHz).
3.4 Medidores Analógicos (ADC)
•
S-Meter (Entrada de señal RX): Pin 36
•
Vúmetro (Entrada de modulación TX): Pin 32
•
Circuito recomendado:PRESET10kR110k3.3V1N4728AC1100nFD21N4148R21MENTRADAAL GPIO DEL ESP32
LCCSALIDA SENOIDALDE CLK 1 o 2
4. Configuración de Arduino IDE
Para que el código compile y funcione, debés configurar el IDE de Arduino exactamente así:
1.
Placa (Board): NodeMCU 32S o ESP32S3 Dev Module.
2.
Configuración de la placa:
•
USB CDC On Boot: CDC (Habilitado)
•
Flash Size: 8MB
•
Partition Scheme: No OTA (Large APP)
•
PSRAM: Disabled
•
Flash Mode: DIO
Librerías necesarias (descargar desde el Gestor de Librerías):
1.
Adafruit_GFX_Library (v1.12.5)
2.
ESP32Encoder (v0.12.0 - Para el control del encoder por hardware)
3.
Etherkit Si5351 (v2.2.0 - Para el sintetizador)
4.
URL adicional del gestor de placas: https://dl.espressif.com/dl/package_esp32_index.json
5. Modo de Uso (Funcionamiento del Panel)
El panel se maneja principalmente con el Encoder Rotativo. Tiene un botón (click) y cuatro tiempos de pulsación prolongada (Hold).
5.1 Tiempos de Pulsación del Botón (Cascada)
Mantené apretado el botón del encoder para entrar a los distintos modos. Mientras mantenés apretado, la pantalla te va indicando a qué modo vas a entrar. Soltá el botón cuando veas el modo que querés.
0 a 1 seg
(Click normal)
Cambia el paso de sintonía (1 kHz / 10 kHz / 100 kHz).
1 a 3 seg
BFO (BLU)
Entra al modo Banda Lateral Única. (Enciende la inyección de 455 kHz ± RIT).
3 a 5 seg
MEM
Entra al modo Memoria (guardar/cargar frecuencias).
5 a 10 seg
CAL
Entra al modo Calibración del cristal.
Más de 10 seg
RESET
Entra al modo Borrado de Calibración.
5.2 Sintonía Normal (Modo AM)
•
Girar el encoder: Mueve la frecuencia. Si girás muy rápido, el sistema acelera automáticamente (salta de 100 kHz en 100 kHz).
•
Click corto: Cambia el dígito que se ilumina en celeste. Podés elegir mover la frecuencia de 1 en 1 kHz, de 10 en 10 kHz, o de 100 en 100 kHz.
5.3 Modo BLU (BFO)
Para escuchar señales de Banda Lateral Única (SSB):
1.
Mantené apretado 1 segundo hasta ver BFO en la pantalla. La palabra AM cambiará a BLU en naranja.
2.
Hacé click cortos para ciclar el paso de sintonía: 100k -> 10k -> 1k -> CLAR -> 100k.
3.
Cuando el paso sea CLAR (Clarificador), los últimos 3 dígitos de la pantalla se pondrán en celeste cuando esté en cero.
4.
Al girar el encoder en CLAR, los últimos 3 dígitos se pondrán naranja. No moverás la frecuencia principal, sino el offset de la inyección de 455 kHz (de 25 en 25 Hz, hasta un máximo de ±1000 Hz). Esto te permite centrar perfectamente la voz del corresponsal sin perder la frecuencia de TX.
5.
Para salir, mantené apretado 1 segundo.
5.4 Memorias (MEM)
1.
Mantené apretado hasta entrar en MEM.
2.
Girá el encoder para elegir uno de los 4 bancos (M1, M2, M3 o M4).
3.
Click corto: Carga la frecuencia guardada en ese banco.
4.
Mantener apretado 3 segundos: Guarda la frecuencia actual en ese banco (la pantalla parpadeará o guardará silenciosamente).
5.5 Calibración (CAL)
El cristal del Si5351 tiene un pequeño error de fábrica. Para corregirlo:
1.
Sintonizá una emisora patrón conocida (por ejemplo, una emisora de onda corta en 7220 kHz).
2.
Entrá al modo CAL (Hold 5 seg).
3.
Girá el encoder (en pasos de 1 Hz) hasta escuchar el cero batido (el momento exacto en que el silbido desaparece y la portadora queda muda).
4.
Mantené apretado 3 segundos para guardar. El equipo calculará el error y lo aplicará.
5.6 Reset de Calibración
Si cambiás el módulo Si5351 o te equivocaste calibrando:
1.
Entrá al modo RESET (Hold 10 seg).
2.
Click corto: Cancela y sale.
3.
Mantener 3 segundos: Borra la calibración de fábrica y vuelve a cero.
6. Transmisión (PTT)
•
Al presionar el PTT, el panel apaga el oscilador de RX (CLK1 y CLK2) y enciende el de TX (CLK0).
•
La pantalla cambia de RX (verde) a TX (rojo).
•
Si la frecuencia en la que estás no está dentro de la banda de 40 metros (7.0 a 7.3 MHz), la pantalla mostrará noTX y el relé no se activará para proteger el equipo.
•
Al soltar el PTT, todo vuelve a modo RX. Si estabas en BLU, el clarificador sigue exactamente donde lo dejaste.
7. Personalización (¡Hacelo Tuyo!)
Este proyecto es de código abierto y podés adaptarlo a tu indicativo. En el código fuente, buscá la función actualizarPantallas() y buscá esta línea:
cpp
tft1.print("LW3DLP");
Reemplazá "LW3DLP" por tu propio indicativo. Por ejemplo: tft1.print("LU1XXX");.
8. Licencia y Agradecimientos
Este proyecto fue concebido, diseñado y programado por Luis Ángel Petitto (LW3DLP).
Este trabajo está bajo la licencia Creative Commons BY-NC-SA 4.0 (Atribución-NoComercial-CompartirIgual).
Esto significa que sos libre de:
•
Compartir: Copiar y redistribuir el material en cualquier medio o formato.
•
Adaptar: Remezclar, transformar y construir a partir del material.
Bajo los siguientes términos:
•
Atribución: Siempre debés dar el crédito correspondiente al autor original (Luis Ángel Petitto / LW3DLP).
•
NoComercial: No podés utilizar el material para fines comerciales (no podés vender este diseño, ni kits armados, ni cobrar por el código).
•
Compartir Igual: Si remezclás, transformás o creás a partir del material, debés distribuir tus contribuciones bajo la misma licencia que el original.
Sobre el espíritu del proyecto: El objetivo de compartir esto libremente es que la comunidad radioaficionada pueda experimentar, aprender y mejorar sus equipos. Se permite su uso, modificación y construcción personal. Queda terminantemente prohibida su venta o integración en productos comerciales sin autorización expresa del autor.
Contacto del Autor: lpetto@gmail.com
¡Que disfrutes de armar y usar este panel, y buenos contactos en la banda de 40 metros! 73.


