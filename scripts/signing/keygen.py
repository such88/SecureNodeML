#!/usr/bin/env python3
"""
scripts/signing/keygen.py
Day 10 — Generate ECDSA P-256 keypair for model signing

Run ONCE at project start:
    python scripts/signing/keygen.py

Output:
    keys/private.pem  ← NEVER commit · NEVER share · production: use HSM
    keys/public.pem   ← commit to repo · embed as bytes in both firmware files

After running:
    openssl ec -in keys/public.pem -pubin -noout -text 2>&1
    → copy the X and Y bytes into:
        firmware/stm32/src/model_verify.c    PUBLIC_KEY_X / PUBLIC_KEY_Y
        firmware/esp32/main/gateway_verify.c PUBLIC_KEY_X / PUBLIC_KEY_Y
"""
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.hazmat.primitives import serialization
import pathlib, sys, textwrap

REPO_ROOT = pathlib.Path(__file__).parent.parent.parent
KEYS_DIR  = REPO_ROOT / "keys"
KEYS_DIR.mkdir(exist_ok=True)

priv_path = KEYS_DIR / "private.pem"
pub_path  = KEYS_DIR / "public.pem"

if priv_path.exists():
    print(f"ERROR: {priv_path} already exists.")
    print("Regenerating invalidates ALL previously signed models.")
    print("Delete it manually if you really want to regenerate.")
    sys.exit(1)

key = ec.generate_private_key(ec.SECP256R1())

priv_path.write_bytes(key.private_bytes(
    serialization.Encoding.PEM,
    serialization.PrivateFormat.PKCS8,
    serialization.NoEncryption()))

pub_path.write_bytes(key.public_key().public_bytes(
    serialization.Encoding.PEM,
    serialization.PublicFormat.SubjectPublicKeyInfo))

print(f"[OK] Private key: {priv_path}  ← NEVER commit this file")
print(f"[OK] Public key:  {pub_path}   ← commit to repo")
print()
print("Next: extract raw public key bytes for firmware embedding:")
print("  openssl ec -in keys/public.pem -pubin -noout -text 2>&1")
print()
print("Copy the X and Y coordinates (32 bytes each) into:")
print("  firmware/stm32/src/model_verify.c    → PUBLIC_KEY_X, PUBLIC_KEY_Y")
print("  firmware/esp32/main/gateway_verify.c → PUBLIC_KEY_X, PUBLIC_KEY_Y")
print()
print("IMPORTANT: both boards must have the same key bytes")
