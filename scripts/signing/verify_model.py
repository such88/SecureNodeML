#!/usr/bin/env python3
"""
scripts/signing/verify_model.py
PC-side signature verification — run after sign_model.py

Usage:
    python scripts/signing/verify_model.py <model.tflite>

Run this before:
  - Flashing the model as a C array
  - Serving the model over OTA HTTP

If this fails, the STM32 will also fail at boot.
"""
import sys, pathlib, hashlib
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes, serialization

def main():
    if len(sys.argv) < 2:
        print("Usage: verify_model.py <model.tflite>"); sys.exit(1)

    model_path = pathlib.Path(sys.argv[1])
    sig_path   = pathlib.Path(str(model_path) + ".sig")
    pub_path   = pathlib.Path(__file__).parent.parent.parent / "keys" / "public.pem"

    for p in [model_path, sig_path, pub_path]:
        if not p.exists():
            print(f"ERROR: {p} not found"); sys.exit(1)

    pub_key    = serialization.load_pem_public_key(pub_path.read_bytes())
    model_data = model_path.read_bytes()
    sig_data   = sig_path.read_bytes()

    try:
        pub_key.verify(sig_data, model_data, ec.ECDSA(hashes.SHA256()))
        sha = hashlib.sha256(model_data).hexdigest()
        print(f"[OK] SIGNATURE VALID")
        print(f"     Model:   {model_path.name} ({len(model_data):,} bytes)")
        print(f"     SHA-256: {sha}")
        print(f"     This is the hash STM32 SPDM will report in GET_MEASUREMENTS")
    except Exception as e:
        print(f"[FAIL] SIGNATURE INVALID: {e}")
        print("Re-run: python scripts/signing/sign_model.py <model.tflite>")
        sys.exit(1)

if __name__ == "__main__":
    main()
