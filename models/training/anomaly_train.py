#!/usr/bin/env python3
"""
models/training/anomaly_train.py
Day 8 — Train anomaly detection autoencoder and convert to INT8 .tflite

Architecture: 4 inputs → Dense(8) → Dense(2)[bottleneck] → Dense(8) → 4 outputs
Trained only on normal data. High reconstruction error = anomaly.

Run on Windows (native Python, not WSL2):
    python models/training/anomaly_train.py

Output:
    models/converted/anomaly_int8.tflite

After this:
    python scripts/signing/sign_model.py models/converted/anomaly_int8.tflite 1
    python scripts/signing/verify_model.py models/converted/anomaly_int8.tflite
    xxd -i models/converted/anomaly_int8.tflite > firmware/stm32/src/anomaly_model_data.cc
    (Then uncomment anomaly_model_data.cc in firmware/stm32/CMakeLists.txt)
"""
import numpy as np
import tensorflow as tf
import pathlib

OUT_DIR = pathlib.Path(__file__).parent.parent / "converted"
OUT_DIR.mkdir(exist_ok=True)

print("=== Day 8: Anomaly autoencoder training ===")
np.random.seed(42)
tf.random.set_seed(42)

# ── Normal sensor data (training set) ────────────────────────────────────
# 4 features: [temperature, vibration, voltage, current] normalised 0-1
# Real deployment: read from ADC/I2C and normalise to this range
normal_data = np.random.normal(
    loc=  [0.50, 0.30, 0.80, 0.60],   # typical operating point
    scale=[0.05, 0.03, 0.02, 0.04],   # natural variation
    size=(2000, 4)
).clip(0.0, 1.0).astype(np.float32)

# ── Anomaly examples (validation only — NOT used in training) ─────────────
anomaly_data = np.array([
    [0.90, 0.80, 0.40, 0.10],  # over-temperature + high vibration
    [0.50, 0.30, 0.20, 0.60],  # voltage dropout
    [0.50, 0.90, 0.80, 0.60],  # bearing failure (high vibration)
    [0.10, 0.05, 0.95, 0.95],  # sensor failure (near-zero temp/vib)
], dtype=np.float32)

# ── Autoencoder architecture ──────────────────────────────────────────────
# Encoder: 4 → 8 → 2 (bottleneck forces compression = learns normal patterns)
# Decoder: 2 → 8 → 4 (reconstructs original signal)
inp  = tf.keras.Input(shape=(4,), name="sensor_input")
enc1 = tf.keras.layers.Dense(8,  activation='relu', name="encoder_1")(inp)
lat  = tf.keras.layers.Dense(2,  activation='relu', name="bottleneck")(enc1)
dec1 = tf.keras.layers.Dense(8,  activation='relu', name="decoder_1")(lat)
out  = tf.keras.layers.Dense(4,  name="reconstruction")(dec1)  # no relu — raw output

model = tf.keras.Model(inputs=inp, outputs=out, name="anomaly_autoencoder")
model.compile(optimizer='adam', loss='mse')
model.summary()

print(f"\nTraining on {len(normal_data)} normal samples (300 epochs)...")
history = model.fit(
    normal_data, normal_data,   # input = output = same (autoencoder)
    epochs=300,
    batch_size=64,
    validation_split=0.1,
    verbose=0
)
final_loss = history.history['loss'][-1]
print(f"Final training loss: {final_loss:.6f}")

# ── Validate anomaly detection ────────────────────────────────────────────
def mse(a, b):
    return float(np.mean((a - b) ** 2))

normal_pred  = model.predict(normal_data[:20], verbose=0)
anomaly_pred = model.predict(anomaly_data,     verbose=0)

normal_errs  = [mse(normal_data[i], normal_pred[i])  for i in range(20)]
anomaly_errs = [mse(anomaly_data[i], anomaly_pred[i]) for i in range(len(anomaly_data))]

avg_normal  = np.mean(normal_errs)
min_anomaly = np.min(anomaly_errs)
ratio = min_anomaly / avg_normal if avg_normal > 0 else 0

print(f"\n=== Anomaly Detection Quality ===")
print(f"Normal  recon error (avg): {avg_normal:.5f}")
print(f"Anomaly recon errors:      {[f'{e:.4f}' for e in anomaly_errs]}")
print(f"Detection ratio:           {ratio:.1f}x  (target: >10x)")

if ratio < 5:
    print("WARNING: low detection ratio. Try training for more epochs.")
elif ratio < 10:
    print("ACCEPTABLE: ratio ok but consider more epochs for robust detection.")
else:
    print("GOOD: strong anomaly detection.")

# Suggested threshold = 5x normal error
threshold = avg_normal * 5
print(f"\nSuggested ANOMALY_THRESHOLD = {threshold:.4f}")
print(f"(Update #define ANOMALY_THRESHOLD in firmware/stm32/src/inference_thread.c)")

# ── Convert to INT8 TFLite ────────────────────────────────────────────────
print("\nConverting to INT8 TFLite...")
converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]

def representative_dataset():
    # Use NORMAL training data for calibration — not anomalies
    for sample in normal_data[:200]:
        yield [np.expand_dims(sample, axis=0)]

converter.representative_dataset = representative_dataset
converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
converter.inference_input_type  = tf.float32
converter.inference_output_type = tf.float32

tflite_bytes = converter.convert()
out_path = OUT_DIR / "anomaly_int8.tflite"
out_path.write_bytes(tflite_bytes)

print(f"Saved: {out_path} ({len(tflite_bytes):,} bytes)")

# ── Verify INT8 model still detects anomalies ─────────────────────────────
print("\nVerifying INT8 model accuracy...")
interp = tf.lite.Interpreter(str(out_path))
interp.allocate_tensors()
inp_det = interp.get_input_details()[0]
out_det = interp.get_output_details()[0]

def infer_tflite(sample):
    interp.set_tensor(inp_det['index'],
                      np.expand_dims(sample, 0).astype(np.float32))
    interp.invoke()
    return interp.get_tensor(out_det['index'])[0]

int8_normal_errs  = [mse(normal_data[i], infer_tflite(normal_data[i]))  for i in range(20)]
int8_anomaly_errs = [mse(anomaly_data[i], infer_tflite(anomaly_data[i])) for i in range(len(anomaly_data))]
int8_ratio = min(int8_anomaly_errs) / np.mean(int8_normal_errs)
print(f"INT8 detection ratio: {int8_ratio:.1f}x  (float32 was {ratio:.1f}x)")

print(f"\n=== Done ===")
print(f"Next steps:")
print(f"  1. python scripts/signing/sign_model.py {out_path} 1")
print(f"  2. python scripts/signing/verify_model.py {out_path}")
print(f"  3. xxd -i {out_path} > firmware/stm32/src/anomaly_model_data.cc")
print(f"  4. Uncomment anomaly_model_data.cc in firmware/stm32/CMakeLists.txt")
print(f"  5. west build -p auto -b disco_f407vg firmware/stm32")
