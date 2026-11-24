"""Calibração do poste pela serial: reinicia o ESP32, roda os testes e resume os resultados.

Uso (com o Serial Monitor fechado):
    ~/.platformio/penv/bin/python tools/calibrar.py [porta] [segundos_de_csv]

Grava o log cru em tools/logs/calibracao-AAAAMMDD-HHMMSS.log e imprime:
  - relatório de ligação dos LDRs (teste de pull) e dos pinos dos LEDs
  - teste óptico: quanto cada LDR sobe com cada LED aceso
  - estatística do fluxo CSV (leituras, ruído, distância do HC-SR04)
  - avisos com o que provavelmente está errado na montagem

Os comandos usados ("p", "o", "c") estão descritos em include/Calibration.h.
"""

import glob
import os
import re
import statistics as st
import sys
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
FIELDS = ("ms,amb,amb_sd,amb_pu,amb_pd,amb_link,loc,loc_sd,loc_pu,loc_pd,loc_link,"
          "lamp_pct,l1u,l1d,l2u,l2d,l3u,l3d,l4u,l4d,dist_cm,echo,night").split(",")

# Limites usados nos avisos (mesma lógica do firmware, em AppConfig.h)
PULL_FLOATING_DIFF = 3000   # pull-up menos pull-down acima disso: pino solto
MIN_DISTANCE_CM = 5.0
LAMP1_OPTICAL_MIN = 150


def capture(port, csv_seconds):
    s = serial.Serial()
    s.port, s.baudrate, s.timeout = port, 115200, 0.1
    s.dtr = s.rts = False
    s.open()
    s.rts = True; time.sleep(0.1); s.rts = False  # pulso no EN: reinicia e mostra o boot
    s.reset_input_buffer()
    buf = bytearray()

    def pump(seconds, until=None):
        end = time.time() + seconds
        while time.time() < end:
            chunk = s.read(4096)
            if chunk:
                buf.extend(chunk)
                if until and until.encode() in buf:
                    return

    print("Aguardando o boot e o diagnóstico...", flush=True)
    pump(90, until="[LOOP]")
    for cmd, wait in (("s", 4), ("p", 3), ("o", 22), ("c", csv_seconds), ("c", 1)):
        print(f"Comando {cmd!r}...", flush=True)
        s.write(cmd.encode())
        pump(wait)
    s.close()
    return bytes(buf).decode("utf-8", "replace").replace("\r", "")


def analyse(text):
    # O driver USB-serial do macOS às vezes repete pedaços antigos ao abrir a porta: começa no boot real
    start = text.rfind("=== ILUMINACAO IOT ===")
    text = text[start:] if start >= 0 else text
    lines = text.split("\n")
    warnings = []

    print("\n== Diagnóstico de boot ==")
    for line in lines:
        if line.startswith("  ") and re.match(r"  \S", line) or "visto pelo LDR" in line:
            print(line)

    print("\n== Varredura dos pinos (comando s) ==")
    for line in lines:
        if line.startswith("[CAL] GPIO"):
            print(line)
            m = re.match(r"\[CAL\] GPIO\s+(\d+) (.*?)\s{2,}.*(preso em \w+)", line)
            # GPIO 0 tem pull-up de fábrica na placa (botão BOOT): HIGH ali é normal
            if m and "livre" in m.group(2) and m.group(1) != "0":
                warnings.append(f"GPIO {m.group(1)} deveria estar livre, mas está {m.group(3)}: há um fio ou "
                                "componente nessa fileira. Confira se não é um fio que deveria ir para outro pino.")
        if line.startswith("[CAL] GPIO 35") and re.search(r"tensao\s+40\d\d", line):
            warnings.append("KY-038: a saída DO (GPIO 35) está sempre em HIGH, então o comparador nunca dispara de "
                            "novo. Gire o trimpot azul até o LED DO do módulo apagar em silêncio.")

    print("\n== Ligação (comando p) ==")
    for line in lines:
        if line.startswith("[CAL] LDR") or line.startswith("[CAL] LED") and "GPIO" in line:
            print(line)

    print("\n== Teste óptico (comando o) ==")
    optical = [l for l in lines if l.startswith("[CAL] LED") and "LDR local" in l]
    for line in optical:
        print(line)
    amb_rises = [int(m.group(1)) for l in optical if (m := re.search(r"LDR amb\s+([+-]\d+)", l))]
    if len(amb_rises) >= 4 and max(amb_rises) - min(amb_rises) < 0.25 * max(1, abs(st.median(amb_rises))):
        warnings.append(
            f"O LDR ambiente sobe quase igual com qualquer LED ({min(amb_rises):+d} a {max(amb_rises):+d}): "
            "é interferência elétrica, não luz. Ele não serve para dizer qual LED está ligado.")

    rows = []
    for line in lines:
        if line.startswith("CAL,"):
            parts = line.split(",")[1:]
            if len(parts) == len(FIELDS):
                rows.append(dict(zip(FIELDS, parts)))
    print(f"\n== Fluxo CSV: {len(rows)} amostras ==")

    def nums(key):
        return [float(r[key]) for r in rows if r[key] not in ("", "-1")]

    for key in ("amb", "amb_sd", "amb_pu", "amb_pd", "loc", "loc_sd", "loc_pu", "loc_pd", "lamp_pct", "dist_cm"):
        v = nums(key)
        if v:
            print(f"  {key:9s} mín {min(v):7.1f}  mediana {st.median(v):7.1f}  máx {max(v):7.1f}  desvio {st.pstdev(v):6.1f}")

    amb = nums("amb")
    amb_pu, amb_pd = nums("amb_pu"), nums("amb_pd")
    if amb and amb_pu and st.median(amb) > 2800 and st.median(amb_pu) - st.median(amb_pd) < 400:
        warnings.append(
            f"LDR ambiente (GPIO 33) em {st.median(amb):.0f} ADC e muito \"duro\" no teste de pull "
            f"({st.median(amb_pu):.0f}/{st.median(amb_pd):.0f}): parece haver dois LDRs ligados no GPIO 33. "
            "O firmware acha que é dia e apaga os LEDs. O LDR do poste deve ir no GPIO 32, o pino vizinho.")

    for name, pu, pd, pin in (("LDR ambiente", "amb_pu", "amb_pd", 33), ("LDR do poste", "loc_pu", "loc_pd", 32)):
        up, down = nums(pu), nums(pd)
        if up and st.median(up) - st.median(down) > PULL_FLOATING_DIFF:
            warnings.append(
                f"{name} (GPIO {pin}): pull-up {st.median(up):.0f} e pull-down {st.median(down):.0f}. "
                "O pino não está ligado a nada: confira se o LDR e o resistor de 10 kΩ estão na mesma fileira do fio "
                f"que vai ao GPIO {pin}, e se o LDR recebe 3V3.")
    amb_sd = nums("amb_sd")
    if amb_sd and st.median(amb_sd) > 15:
        warnings.append(
            f"Ruído do LDR ambiente dentro de uma leitura: {st.median(amb_sd):.0f} ADC. Normal com lâmpada de "
            "sala (cintilação de 120 Hz); o teste óptico já usa média longa para contornar.")

    dist = nums("dist_cm")
    if dist and len(dist) > 10 and st.pstdev(dist) < 1.0:
        warnings.append(
            f"HC-SR04 mediu sempre {st.median(dist):.0f} cm ({len(dist)} amostras, sem variação). Objeto fixo na "
            f"frente (fio, borda da protoboard?) conta como presença porque a faixa começa em {MIN_DISTANCE_CM:.0f} cm: "
            "a luz fica presa no máximo. Libere a frente do sensor ou aumente MIN_DISTANCE_CM.")

    probes = {tuple(r[k] for k in FIELDS[12:20]) for r in rows}
    if probes == {("1", "0") * 4}:
        warnings.append(
            "Teste elétrico dos LEDs: os 4 pinos sobem com o pull-up, ligados ou não. A tensão do LED "
            "(~1,7 V) fica acima do limite de HIGH do ESP32, então esse teste não distingue LED solto.")

    print("\n== Avisos ==")
    for w in list(dict.fromkeys(warnings)) or ["Nenhum problema evidente."]:
        print(" -", w)


def main():
    ports = sys.argv[1:2] or glob.glob("/dev/cu.usbserial-*") or glob.glob("/dev/ttyUSB*")
    if not ports:
        sys.exit("Nenhuma porta serial encontrada. Passe a porta como argumento.")
    csv_seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 20
    text = capture(ports[0], csv_seconds)
    os.makedirs(os.path.join(HERE, "logs"), exist_ok=True)
    path = os.path.join(HERE, "logs", time.strftime("calibracao-%Y%m%d-%H%M%S.log"))
    with open(path, "w") as f:
        f.write(text)
    print("Log salvo em", path)
    analyse(text)


if __name__ == "__main__":
    main()
