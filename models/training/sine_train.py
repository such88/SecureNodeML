#!/usr/bin/env python3
"""
models/training/sine_train.py
Day 3 — Train a sine regression model and convert to INT8 .tflite

Run on Windows (native Python, not WSL2):
    python models/training/sine_train.py

Output:
    models/converted/sine_int8.tflite

After this:
    python scripts/signing/sign_model.py models/converted/sine_int8.tflite 1
    xxd -i models/converted/sine_int8.tflite > firmware/stm32/src/sine_model_data.cc
"""
import numpy as np
import tensorflow as tf
import pathlib

OUT_DIR = pathlib.Path(__file__).parent.parent / "converted"
OUT_DIR.mkdir(exist_ok=True)

print("=== Day 3: Sine model training ===")

# ── Training data ─────────────────────────────────────────────────────────
x = np.linspace(0, 2 * np.pi, 1000).astype(np.float32)
y = np.sin(x)

# ── Model: 2 hidden Dense(16) layers ─────────────────────────────────────
# This is linear regression (y = w*x + b) done 16 times per layer,
# stacked 2 layers deep, with relu between layers.
model = tf.keras.Sequential([
    tf.keras.layers.Dense(16, activation='relu', input_shape=(1,)),
    tf.keras.layers.Dense(16, activation='relu'),
    tf.keras.layers.Dense(1)  # no relu on output — sin(x) can be negative
])
model.compile(optimizer='adam', loss='mse')

print("Training (500 epochs)...")
history = model.fit(x, y, epochs=500, batch_size=32, verbose=0)
final_loss = history.history['loss'][-1]

pred = float(model.predict(np.array([[1.0]]), verbose=0)[0][0])
print(f"Final loss:       {final_loss:.6f}")
print(f"Prediction x=1.0: {pred:.4f}  (sin(1.0) = {np.sin(1.0):.4f})")

# ── Convert to INT8 TFLite ────────────────────────────────────────────────
print("Converting to INT8 TFLite...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]

# Calibration dataset — 200 samples across full input range
x_cal = np.linspace(0, 2 * np.pi, 200).astype(np.float32)
def representative_dataset():
    for v in x_cal:
        yield [np.array([[v]], dtype=np.float32)]

converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type  = tf.float32
converter.inference_output_type = tf.float32

tflite_bytes = converter.convert()
out_path = OUT_DIR / "sine_int8.tflite"
out_path.write_bytes(tflite_bytes)

print(f"Saved: {out_path} ({len(tflite_bytes):,} bytes)")
print()
print("Next steps:")
print(f"  python scripts/signing/sign_model.py {out_path} 1")
print(f"  xxd -i {out_path} > firmware/stm32/src/sine_model_data.cc")

