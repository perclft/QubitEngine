import pytest
from qubit_engine import core

def test_qec_decoders_surface_code():
    # Verify we can use DecoderType enum
    assert hasattr(core, "DecoderType")
    assert hasattr(core.DecoderType, "MWPM")
    assert hasattr(core.DecoderType, "UnionFind")

    # Construct SurfaceCode using DecoderType.UnionFind
    sc = core.SurfaceCode(3, core.DecoderType.UnionFind)
    success = sc.simulate(5, 0.0)
    assert success is True

def test_qec_decoders_color_code():
    # Construct ColorCode using DecoderType.UnionFind
    cc = core.ColorCode(3, core.DecoderType.UnionFind)
    success = cc.simulate(5, 0.0)
    assert success is True
