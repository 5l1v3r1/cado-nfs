#define BL_TESTING
#include "blocklanczos.c"

struct extraction_check_data {
    uint64_t A[64];
    uint64_t S;
    uint64_t B[64];
    uint64_t T;
};

/* Extracted from a magma run, only picking a few steps */
struct extraction_check_data data[] = {
    { /* step 1 */
        { /* in */
0x3428D3A986B4C199, 0xA5AC66E8EADC9E4C, 0x911C6C2B74D5E012, 0x6F6525FFC76E090B,
0xF892DB6667215F05, 0x7A7CD3AFC9132C00, 0x6B9943321344FF42, 0xB19A9BF4F02A4581,
0x9105C351162093D9, 0x34FDD9D59E1FC152, 0x5ABE186096DBCCF2, 0x9C0BC328E9A17C7A,
0xC2704CACA5C6E952, 0xE519AAAF2B95D864, 0x607E6F3877A87ED5, 0x34B5A96DC954B747,
0x1B675E277B52E34, 0xC977272290D416A8, 0x7F3EE0227A53B24F, 0xFD952F68FAE8468A,
0xD23763B78047A627, 0x555FB30CA4A94999, 0xFFCD953A561E944E, 0x5CA5212B20AB7C07,
0x3651AD91E901F878, 0x9F64AAC2CC4D675B, 0xB0B4CC4DCA61571D, 0xCAD02053BF0CAA22,
0x5D2E6DC8184F47C4, 0x88D1163289AD7896, 0xAB8EC640C74DC8BE, 0x7625B71EEF3A9EAB,
0x990019630D90A32D, 0x73F7D06FAAD7207C, 0x7C5B16AA8430B2B8, 0xA4A95D0694E8F82F,
0xE3B1FE80A95043C8, 0xA48E806720DFFCFF, 0x1490DF635E09879A, 0x97EA3E94131132AB,
0x5698584991FBCBF9, 0x167864D4E23A69F3, 0xF7126ADCF54B500E, 0xE77035D91708F694,
0x749B79DFA06106B1, 0xFAE1BE909BBFE00E, 0x2784175A54155B77, 0x43D72072C764ABB1,
0x999FB01EA1FAAB48, 0x58BF94A650374C90, 0x7A17C022D6FFC72E, 0x5A6313AC50646EE7,
0x8E979F562D3FF6F4, 0x7B2A2A9A9697D62B, 0xB248AA862B625228, 0x1A13F1FA6CC986D2,
0x3421CC93526F21CE, 0x48FCEF92CB541468, 0x11105FEC93ECAA0B, 0x72BF20057ACE0C78,
0x2DEF37C797FC8FB5, 0x59647C3EC54CE2FB, 0x6A2EBD1698FE7478, 0x512CB96E5A3996,
        }, 0xFFFFFFFFFFFFFFFF,
        { /* out */
0x4A8DF93EBF170B5E, 0x24AEDBB214AE2C1, 0x44F8CB3E5448AA21, 0x5FB6B81CE78205D1,
0x1AD2AFECC398ABA9, 0x1F181A10D2F746B4, 0x4157509578E7FE8B, 0x13050DCE2084C07A,
0x4D4A45FB253E4019, 0x4E1CC2C53C422677, 0x42B00E98803F9A68, 0x44204ACC53039455,
0x5BB05E83E0FB7C40, 0x481F988382FD3256, 0xA509EC24CCD51E2, 0x429F4F2A7B248CD6,
0x4D13BAEF7A217C61, 0x5C5E7B3E1A521F6B, 0x50CC6DA88CCCE5E1, 0x4E72B9BC6B247516,
0x57B15FA327F23531, 0x5BF4096AD9D9B560, 0x132A19E122F67266, 0xE29537FD17470F8,
0x12799A9EA2B8891B, 0x8EA65464B5BA839, 0x586F9C72F414430D, 0xB99BF3D122FC241,
0x10411C925CA38A65, 0xD645B86255993CB, 0x1D24DF5ED6A9D87C, 0x55568C3C5A43439,
0x13DEA84588D13342, 0x56B3E4CEF7B3F187, 0x88B1FBB6B8B0ADD, 0x442B09BE49AF8D9F,
0x595EA82C5D8A056F, 0x55B66BFC0CFF8117, 0x18124663C6E14B90, 0x29A5AEB15D7FD2,
0x5EC180AC6AFE8197, 0x4B6102646993DE34, 0x54A4A4C65E14D592, 0x170F03DFD7FFCBF,
0x1B47F8047DDB7069, 0x44117CB38A0F001B, 0x4FEDB862E2969B47, 0x1246DD934D09621F,
0x45BA738E9D91A0C1, 0x4B37907F064BA15A, 0x5F22D431E426A2C9, 0x1341409D0FC6A327,
0xD032873893BF67C, 0x45C74EAA67F81C0C, 0x4968DB11B72E4156, 0x18A145270A34941D,
0x597F5A31E87111E8, 0x4A0ED30309F8D6BB, 0x1C35652AE09B0B2C, 0x4FD653546EAB7339,
0x58C9573557610B8, 0x0, 0x4B67673A043FBF4D, 0x0,
        }, 0x5FFFFFFFFFFFFFFF,
    },
    { /* step 2 */
        { /* in */
0x503216E2816640A0, 0xDAE6F786B8A09500, 0x364940F9F05E8C34, 0x44468CC9E54CFDA8,
0xD6C8D541D6AC31A4, 0xE7E4AD0D81621A9D, 0x42295D83A73A53C0, 0x577EAA0E9860A079,
0x17E1AC08F2974D5A, 0xF086D342D61AE860, 0xE7EDD8111D0A010E, 0x76923958EDF0CB2C,
0x615AB625B8FDF07A, 0x941A1C2E71CA9298, 0x5A604A5E11521B49, 0x3C636F3EB828BA8E,
0xCAC2F55B07F31100, 0x7348A135CD8D6765, 0x2238891242AE111D, 0xABD7FFCDA23EB65C,
0x3F76C6057B095B44, 0xF9A7C5862A2D98F3, 0x2F24FD4CF48178AD, 0xA37E1B84EFC73912,
0xB6B20149E6936C69, 0x751EADABC7BD0350, 0x92FEADC86FC30E58, 0x78563D3F74B29C82,
0xF68244B8B850F796, 0xDCBD10E23DF8B94E, 0x2B54E8DECFD62B1C, 0x7AE25C4953CA9BFF,
0xFA0FD82A8B1B147C, 0xA6EE66436A25E2C3, 0x26BD2B0448FAF0A2, 0x184EFCF1DF49E9AC,
0xF02A82085807CC04, 0x6F4BD293A02B005, 0xC5FE628AE5494A1D, 0xB946B7C876A8004F,
0x34BC01A40FEF8A72, 0x180EE6D60098D283, 0xE7A04EAA9E79B17B, 0x83493C2DCECCEDE8,
0xDAF398A9A8C93E53, 0xC6EA2AEE4E4B99A2, 0xAB0B864BD079C656, 0x37C7D2B9467F17BA,
0x8957D80520288544, 0x417BF2DB9FB9BA8B, 0xD0D983EF6EF806AA, 0xF3866B5F268634D4,
0x36C711646F9C3881, 0x37623576A5F4C5E3, 0x2937B8EACC9BD5BE, 0x8C1CB566B5290F32,
0x316BCCC042FA15A0, 0x1A38FC27D5DF4DF6, 0xA0B0A5663350ADBC, 0x12C15289E879C002,
0xAB3C9399BF32EB97, 0x1578C597DBFE9E24, 0x400E3451BA235EFB, 0x148D7CD335A92632,
        }, 0x5FFFFFFFFFFFFFFF,
        { /* out */
0x820324AAB5B5A3B4, 0x46B7EA3F115C3AA, 0x200723ACDC344B09, 0x611C83C07E1606C6,
0x652D1F7B0E433701, 0x446331FDA0BD8803, 0xA62A07A5154F8948, 0x222F9BAC747F558B,
0x602038F3365C1ED7, 0xA21930F8B877951F, 0x80D7F58717C84798, 0x47A1DFF3A579B964,
0xE5560DAEE40B8B90, 0xE78D25F4D7B56811, 0x6452BA7DC788A486, 0xA0C92A845FAF5A63,
0x276B6A1ED420BAF3, 0x21D8CDAA646092D8, 0x67969C98FEC0A3EF, 0x20B09694D6E8DDE0,
0xC5CC05235B202BAF, 0xC4E1B78E2BDBAAA5, 0xA4242774842E0FD0, 0x116FA99392CE421,
0xC099CAE9F1B0EC43, 0x26FB19441E3CE518, 0xE4931350AA4FFDDD, 0x44EDDF5846B4821C,
0x64A6CB66139DA7CF, 0x25F119F2C5A61BAB, 0x41AEAC7CE91FF08E, 0x3176539E54D7A27,
0xC1E9A36A81904D72, 0x40878E5B30331D13, 0xE2811CB05269F4E4, 0xE6621C63C9A752B5,
0x65210106ECCD6B30, 0x79DC70DF1527BF7, 0x826A050B7F406B38, 0xC47F378421AEBFEF,
0xE4FAB7F1BE723CFC, 0x1B49BA31DE9C8DE, 0xC62D91EEC87E3C53, 0x82546A0E7B87D992,
0xE6E8278C2EAC4FB2, 0x20E9D981C0E1E727, 0x809EE82099830C02, 0x45CF672359AE4C88,
0x4C6A4B7AF21AEB7, 0x225FC1CAD68554E7, 0x65E7CEA2D8D4349C, 0x2752F5E14B13A2DA,
0x667A4BA0A78E5608, 0xC17437D97A6909F2, 0x22BFB9C92A33D422, 0xA0C5F3277F3EAC00,
0x242C8231E0973818, 0xC05A1C6C82052AC1, 0x811D95B83E757872, 0x0,
0x0, 0xA1DE311C364FF3DC, 0x4234959F5D347938, 0xA6A05DCD0570B641,
        }, 0xE7FFFFFFFFFFFFFF,
    },
    { /* step 64 */
        { /* in */
0x2BC23AB4F350C6D6, 0x13EA2EEEFD9C3A39, 0xEC6B52D4A0D02F71, 0xFE1EBB70CFF159FA,
0x123CABBD9B6724DF, 0x269F649293A3FC0E, 0x71F725AA18ADA4DD, 0x428F31C8283CCDD9,
0x34D17B2BA250A78C, 0x72D776B91C205707, 0xFAE5332EF3291BF5, 0xC7929E7D0D52C4AE,
0x856F1BB18FFA762A, 0xF92BBCEDA1AA5176, 0xD2D270440C28BAA9, 0x7301F3EA7ADB49E1,
0xB82340FF2F538478, 0x2035A4456F45B830, 0x2B5E52600FCA00D2, 0xA807E1D2804CF4C2,
0x757E19F08EE1998F, 0xAE3149D1803076F8, 0xD78DC94DFC9F991D, 0xC5EF1B80ED4B06E,
0x245244CFAF073C3B, 0x200E525831979539, 0x2C2217E495D75A0A, 0x64B7CB9319D7DADA,
0xE83744433E408673, 0x64C57F97B343A587, 0x467E08AC042840B, 0x1D0B4B0F6578353F,
0xBA9CD4A6B9633B10, 0x24BC981FF9098562, 0xC84BA21FA5436C17, 0x18BA2ED6C3C1AFD2,
0x4D24B9AE2AB91A3D, 0x3756B6F10495BF5B, 0x6EDF9228177FE88E, 0xFEB925B96DB9B2F7,
0x3EB92290ACF895D8, 0xE035A36CAE049F1F, 0x1758E4A935022A62, 0xEA88D01AA870391B,
0x48E768732694FF8D, 0xDC7A57BC608AE7FB, 0xCE46FC01FBEDC324, 0x98C3CE7748CAA818,
0x58A93C4F86BB7E4, 0x6C5BF06CDF9D7AEF, 0xA41052737ADE16F8, 0x44CB2DCF82D430BE,
0x81E627EB19B64B78, 0x3DD0339A5C333456, 0xFB7AF46461945747, 0x44B999CB28404FE3,
0x9C7104308054B843, 0x32404DE10064CEBB, 0x81AF65F2EDF0192C, 0x3962F9DD94AD240D,
0x4B60A5A98051E75A, 0x2A660BE33F3FA76D, 0xD0CA7AD43850EECC, 0x4554EA8510697C0C,
        }, 0x7FFFFFFFFFFFFFFF,
        { /* out */
0x21A9, 0x8000000000000020, 0x80000000000022B4, 0x281,
0x80000000000021A4, 0x80000000000021B7, 0x0, 0x800000000000003D,
0x231, 0x30C, 0x0, 0x0,
0x0, 0x2035, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0x0,
0x0, 0x0, 0x0, 0xB6,
        }, 0x80000000000023BF,
    },
};

int main()
{
    int fail = 0;
    for(size_t i = 0 ; i < (sizeof(data)/sizeof(data[0])) ; i++) {
        uint64_t B[64];
        uint64_t T = extraction_step(B, data[i].A, data[i].S);
        if (memcmp(B, data[i].B, sizeof(B)) != 0 || T != data[i].T) {
            fprintf(stderr, "failed check number %zu\n", i);
            fail++;
        }
    }
    return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
