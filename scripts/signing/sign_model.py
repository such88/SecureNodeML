#!/usr/bin/env python3
"""
scripts/signing/sign_model.py
Day 10+ — ECDSA P-256 sign a .tflite model file

Usage:
    python scripts/signing/sign_model.py <model.tflite> [version]

Examples:
    python scripts/signing/sign_model.py models/converted/anomaly_int8.tflite 1
    python scripts/signing/sign_model.py models/converted/anomaly_int8.tflite 2

Rules:
    - ALWAYS sign the .tflite file, NOT the .keras file
    - ALWAYS sign AFTER quantization (int8 bytes differ from float32)
    - Version must be strictly increasing (anti-rollback on STM32)
    - In production: private key in HSM, not on developer laptop

Output:
    <model.tflite>.sig  — DER-encoded ECDSA P-256 signature
"""
import sys, pathlib, hashlib
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import hashes, serialization

def main():
    if len(sys.argv) < 2:
        print("Usage: sign_model.py <model.tflite> [version=1]")
        sys.exit(1)

    model_path = pathlib.Path(sys.argv[1])
    version    = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    key_path   = pathlib.Path(__file__).parent.parent.parent / "keys" / "private.pem"

    if not model_path.exists():
        print(f"ERROR: {model_path} not found")
        print("Run anomaly_train.py first")
        sys.exit(1)

    if not key_path.exists():
        print(f"ERROR: {key_path} not found")
        print("Run scripts/signing/keygen.py first")
        sys.exit(1)

    key        = serialization.load_pem_private_key(key_path.read_bytes(), password=None)
    model_data = model_path.read_bytes()
    sha256     = hashlib.sha256(model_data).hexdigest()
    signature  = key.sign(model_data, ec.ECDSA(hashes.SHA256()))

    sig_path = pathlib.Path(str(model_path) + ".sig")
    sig_path.write_bytes(signature)

    print(f"[OK] Model:     {model_path}")
    print(f"     Size:      {len(model_data):,} bytes")
    print(f"     SHA-256:   {sha256}")
    print(f"     Version:   {version}")
    print(f"[OK] Signature: {sig_path} ({len(signature)} bytes DER)")
    print()
    print("Verify with:")
    print(f"  python scripts/signing/verify_model.py {model_path}")
    print()
    print("Serve for OTA:")
    print(f"  python -m http.server 8080 --directory {model_path.parent}")

if __name__ == "__main__":
    main()
