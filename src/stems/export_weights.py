#!/usr/bin/env python3
"""Spleeter 4stems checkpoint (dumped to .npz) -> flat float16 file (79 MB; hs-stems widens to float32 on load) for hs-stems.
Per stem (vocals, drums, bass, other), in this order:
  6 x encoder: conv kernel [5,5,ci,co], bias[co], bn scale[co], bn shift[co]
  6 x decoder: convT kernel [5,5,co,ci], bias[co], bn scale[co], bn shift[co]
  final conv kernel [4,4,1,2], bias[2]
BN is folded: scale = gamma/sqrt(var+1e-3), shift = beta - mean*scale (Keras eps)."""
import sys, numpy as np
w = np.load(sys.argv[1]); out = open(sys.argv[2], 'wb')
nm = lambda b, i: b if i == 0 else f'{b}_{i}'
def bn(i):
    g, b, m, v = (w[f"{nm('batch_normalization', i)}/{k}"] for k in ('gamma', 'beta', 'moving_mean', 'moving_variance'))
    s = g / np.sqrt(v + 1e-3); return [s, b - m * s]
out.write(b'SPH4')
for s in range(4):
    arrs = []
    for l in range(6):
        c = nm('conv2d', 7 * s + l); arrs += [w[c + '/kernel'], w[c + '/bias']] + bn(12 * s + l)
    for l in range(6):
        c = nm('conv2d_transpose', 6 * s + l); arrs += [w[c + '/kernel'], w[c + '/bias']] + bn(12 * s + 6 + l)
    c = nm('conv2d', 7 * s + 6); arrs += [w[c + '/kernel'], w[c + '/bias']]
    for a in arrs: out.write(np.ascontiguousarray(a, dtype='<f2').tobytes())
print('bytes', out.tell())
