# Genera include/lora_key.h (clave AES-128 del enlace LoRa) si todavía no existe.
# Gateway y nodo deben compilarse con el MISMO archivo: al subir los dos desde esta
# carpeta ya la comparten. No se versiona (.gitignore). Ver docs/LORA.md
Import("env")  # noqa: F821 (lo define PlatformIO)

import os
import secrets

path = os.path.join(env.subst("$PROJECT_INCLUDE_DIR"), "lora_key.h")  # noqa: F821

if not os.path.exists(path):
    key = ", ".join(f"0x{b:02x}" for b in secrets.token_bytes(16))
    with open(path, "w", encoding="utf-8") as f:
        f.write(
            "#pragma once\n\n"
            "// Clave del enlace LoRa (AES-128). Generada por scripts/lora_key.py.\n"
            "// SECRETA: quien la tenga puede controlar el motor. No se versiona.\n"
            "// Gateway y nodo deben tener la misma; si se cambia, subir los dos.\n"
            "#include <stdint.h>\n\n"
            f"static const uint8_t LORA_KEY[16] = {{{key}}};\n"
        )
    print("[lora] Clave nueva en include/lora_key.h: sube el gateway Y el nodo con ella")
