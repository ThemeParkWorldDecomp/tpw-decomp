"""Combined coverage module for exactly the 18 requested functions."""
import test_VanillaTime, test_Colours, test_HLSprite, test_TPOptions
TESTS = test_VanillaTime.TESTS + test_Colours.TESTS + test_HLSprite.TESTS + test_TPOptions.TESTS
def setup(runner):
    # Start with the common boundary fakes, then retain Colours' ctor/palette helper.
    test_TPOptions.setup(runner)
    test_Colours.setup(runner)
    # The array helper stub must support both CText's wrapper array and HLSprite.
    import tpwtest as T
    from batch3_support import pl
    e = runner.emu
    def dtor(e):
        assert T.s16(e.r(4)) == -1
        a = e.r(3); e.b3_trace.append((41, pl(e, a))); e.set_r(3, a)
    e.stub("__dt__11CSpriteListFv", dtor)
    def destroy(e, call):
        a, dtor, size, count = [e.r(i) for i in range(3, 7)]
        if a == test_HLSprite.ARRAY:
            assert (size, count) == (40, 16)
            assert e.trap_to_import[e.u32(dtor)]["name"] == "__dt__11CSpriteListFv"
        else:
            assert (size, count, e.u32(dtor)) == (24, 52, e.func("__dt__20InterfaceFontWrapperFv"))
        for i in range(count - 1, -1, -1): call(dtor, a + size * i, 0xffff)
    T.stub_cb(e, "__destroy_arr", destroy)
