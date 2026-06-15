# Copyright © 2023 CCP ehf.

__author__ = 'snorri.sturluson'

from . import blueunittest
import blue
import hashlib

class TestMd5(blueunittest.TestCase):
    def _md5_checksum(self, value):
        m = hashlib.md5()
        m.update(value)
        checksum = m.hexdigest()
        return checksum

    def assertChecksumsEqual(self, value):
        blueChecksum = blue.md5_checksum(value)
        md5Checksum = self._md5_checksum(value)
        self.assertEqual(blueChecksum, md5Checksum)

    def testEmptyString(self):
        self.assertChecksumsEqual(b"")

    def testShortString(self):
        self.assertChecksumsEqual(b"just a string")

    def testLongString(self):
        longString = b"""
type: EveStation2
name: "gs5"
highDetailMesh:
    type: BlueObjectProxy
    object:
        type: Tr2Mesh
        geometryResPath: "res:/dx9/model/station/gallente/gs5/gs5.gr2"
        opaqueAreas:
        -   type: Tr2MeshArea
            name: "Hull"
            index: 1
            effect:
                type: Tr2Effect
                name: "Armor"
                effectFilePath: "res:/graphics/effect/managed/space/spaceobject/v3/doubleaomglowv3.fx"
                parameters:
                -   type: Tr2Vector4Parameter
                    name: "MaterialDiffuseColor"
                    value: [0.5568628, 0.5333334, 0.4901961, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularFactors"
                    value: [0.25, 8.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "FresnelFactors"
                    value: [2.5, 0.5, 0.0, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors"
                    value: [0.1, 0.6, 0.75, 0.44]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularCurve"
                    value: [80.0, 250.0, 0.6, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "AOFactors"
                    value: [1.0, 1.0, 1.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskDiffuseColor"
                    value: [0.2901961, 0.3764706, 0.3803922, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionColor"
                    value: [0.3058824, 0.4156863, 0.4156863, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors"
                    value: [0.15, 1.0, 0.7, 0.85]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularCurve"
                    value: [60.0, 320.0, 0.35, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularFactors"
                    value: [0.1, 20.0, 2.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialReflectionColor"
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors2"
                    value: [6.9000001, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskGlowColor"
                    value: [2.9597001, 2.6347134, 1.77582, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialGlowColor"
                    value: [1.7874266, 2.9597001, 2.4141867, 1.0]
                resources:
                -   type: TriTextureParameter
                    name: "NormalMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_n.dds"
                -   type: TriTextureParameter
                    name: "PgsMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_pgs.dds"
                -   type: TriTextureParameter
                    name: "AoMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_ao.dds"
                -   type: TriTextureParameter
                    name: "DiffuseMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_d.dds"
        transparentAreas:
        -   type: Tr2MeshArea
            name: "Glass"
            effect:
                type: Tr2Effect
                name: "Glass"
                effectFilePath: "res:/graphics/effect/managed/space/spaceobject/v3/doubleglassv3.fx"
                parameters:
                -   type: Tr2Vector4Parameter
                    name: "MaterialDiffuseColor"
                    value: [0.1921569, 0.2901961, 0.254902, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularFactors"
                    value: [0.1, 1.0, 0.5156, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "FresnelFactors"
                    value: [1.0, 1.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors"
                    value: [1.5, 3.0, 0.5195, 0.5]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularCurve"
                    value: [111.1172028, 121.1718979, 8.28125, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskDiffuseColor"
                    value: [0.2862745, 0.2745098, 0.2509804, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionColor"
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors"
                    value: [0.1, 0.6, 0.75, 0.44]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularCurve"
                    value: [10000.0, 10.0, 0.6, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularFactors"
                    value: [0.25, 8.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialReflectionColor"
                    value: [0.4745098, 0.4745098, 0.4745098, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskGlowColor"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialGlowColor"
                    value: [0.0, 0.0, 0.0, 0.0]
                resources:
                -   type: TriTextureParameter
                    name: "NormalMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_n.dds"
                -   type: TriTextureParameter
                    name: "PgsMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_pgs.dds"
                -   type: TriTextureParameter
                    name: "DiffuseMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_d.dds"
mediumDetailMesh:
    type: BlueObjectProxy
    object:
        type: Tr2Mesh
        geometryResPath: "res:/dx9/model/station/gallente/gs5/gs5_mediumdetail.gr2"
        opaqueAreas:
        -   type: Tr2MeshArea
            name: "Hull"
            index: 1
            effect:
                type: Tr2Effect
                name: "Armor"
                effectFilePath: "res:/graphics/effect/managed/space/spaceobject/v3/doubleaomglowv3.fx"
                parameters:
                -   type: Tr2Vector4Parameter
                    name: "MaterialDiffuseColor"
                    value: [0.5568628, 0.5333334, 0.4901961, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularFactors"
                    value: [0.25, 8.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "FresnelFactors"
                    value: [2.5, 0.5, 0.0, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors"
                    value: [0.1, 0.6, 0.75, 0.44]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularCurve"
                    value: [80.0, 250.0, 0.6, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "AOFactors"
                    value: [1.0, 1.0, 1.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskDiffuseColor"
                    value: [0.2901961, 0.3764706, 0.3803922, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionColor"
                    value: [0.3058824, 0.4156863, 0.4156863, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors"
                    value: [0.15, 1.0, 0.7, 0.85]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularCurve"
                    value: [60.0, 320.0, 0.35, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularFactors"
                    value: [0.1, 20.0, 2.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialReflectionColor"
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors2"
                    value: [6.9000001, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskGlowColor"
                    value: [2.9597001, 2.6347134, 1.77582, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialGlowColor"
                    value: [1.7874266, 2.9597001, 2.4141867, 1.0]
                resources:
                -   type: TriTextureParameter
                    name: "NormalMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_n_mediumdetail.dds"
                -   type: TriTextureParameter
                    name: "PgsMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_pgs_mediumdetail.dds"
                -   type: TriTextureParameter
                    name: "AoMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_ao_mediumdetail.dds"
                -   type: TriTextureParameter
                    name: "DiffuseMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_d_mediumdetail.dds"
        transparentAreas:
        -   type: Tr2MeshArea
            name: "Glass"
            effect:
                type: Tr2Effect
                name: "Glass"
                effectFilePath: "res:/graphics/effect/managed/space/spaceobject/v3/doubleglassv3.fx"
                parameters:
                -   type: Tr2Vector4Parameter
                    name: "MaterialDiffuseColor"
                    value: [0.1921569, 0.2901961, 0.254902, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularFactors"
                    value: [0.1, 1.0, 0.5156, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "FresnelFactors"
                    value: [1.0, 1.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors"
                    value: [1.5, 3.0, 0.5195, 0.5]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularCurve"
                    value: [111.1172028, 121.1718979, 8.28125, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskDiffuseColor"
                    value: [0.2862745, 0.2745098, 0.2509804, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionColor"
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors"
                    value: [0.1, 0.6, 0.75, 0.44]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularCurve"
                    value: [10000.0, 10.0, 0.6, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularFactors"
                    value: [0.25, 8.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialReflectionColor"
                    value: [0.4745098, 0.4745098, 0.4745098, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskGlowColor"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialGlowColor"
                    value: [0.0, 0.0, 0.0, 0.0]
                resources:
                -   type: TriTextureParameter
                    name: "NormalMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_n_mediumdetail.dds"
                -   type: TriTextureParameter
                    name: "PgsMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_pgs_mediumdetail.dds"
                -   type: TriTextureParameter
                    name: "DiffuseMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_d_mediumdetail.dds"
lowDetailMesh:
    type: BlueObjectProxy
    object:
        type: Tr2Mesh
        geometryResPath: "res:/dx9/model/station/gallente/gs5/gs5_lowdetail.gr2"
        opaqueAreas:
        -   type: Tr2MeshArea
            name: "Hull"
            index: 1
            effect:
                type: Tr2Effect
                name: "Armor"
                effectFilePath: "res:/graphics/effect/managed/space/spaceobject/v3/doubleaomglowv3.fx"
                parameters:
                -   type: Tr2Vector4Parameter
                    name: "MaterialDiffuseColor"
                    value: [0.5568628, 0.5333334, 0.4901961, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularFactors"
                    value: [0.25, 8.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "FresnelFactors"
                    value: [2.5, 0.5, 0.0, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors"
                    value: [0.1, 0.6, 0.75, 0.44]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularCurve"
                    value: [80.0, 250.0, 0.6, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "AOFactors"
                    value: [1.0, 1.0, 1.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskDiffuseColor"
                    value: [0.2901961, 0.3764706, 0.3803922, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionColor"
                    value: [0.3058824, 0.4156863, 0.4156863, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors"
                    value: [0.15, 1.0, 0.7, 0.85]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularCurve"
                    value: [60.0, 320.0, 0.35, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularFactors"
                    value: [0.1, 20.0, 2.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialReflectionColor"
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors2"
                    value: [6.9000001, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskGlowColor"
                    value: [2.9597001, 2.6347134, 1.77582, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialGlowColor"
                    value: [1.7874266, 2.9597001, 2.4141867, 1.0]
                resources:
                -   type: TriTextureParameter
                    name: "NormalMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_n_lowdetail.dds"
                -   type: TriTextureParameter
                    name: "PgsMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_pgs_lowdetail.dds"
                -   type: TriTextureParameter
                    name: "AoMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_ao_lowdetail.dds"
                -   type: TriTextureParameter
                    name: "DiffuseMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_d_lowdetail.dds"
        transparentAreas:
        -   type: Tr2MeshArea
            name: "Glass"
            effect:
                type: Tr2Effect
                name: "Glass"
                effectFilePath: "res:/graphics/effect/managed/space/spaceobject/v3/doubleglassv3.fx"
                parameters:
                -   type: Tr2Vector4Parameter
                    name: "MaterialDiffuseColor"
                    value: [0.1921569, 0.2901961, 0.254902, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularFactors"
                    value: [0.1, 1.0, 0.5156, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "FresnelFactors"
                    value: [1.0, 1.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors"
                    value: [1.5, 3.0, 0.5195, 0.5]
                -   type: Tr2Vector4Parameter
                    name: "MaterialSpecularCurve"
                    value: [111.1172028, 121.1718979, 8.28125, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskDiffuseColor"
                    value: [0.2862745, 0.2745098, 0.2509804, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionColor"
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors"
                    value: [0.1, 0.6, 0.75, 0.44]
                -   type: Tr2Vector4Parameter
                    name: "MaskReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularCurve"
                    value: [10000.0, 10.0, 0.6, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskSpecularFactors"
                    value: [0.25, 8.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialReflectionColor"
                    value: [0.4745098, 0.4745098, 0.4745098, 1.0]
                -   type: Tr2Vector4Parameter
                    name: "ReflectionFactors2"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaskGlowColor"
                    value: [0.0, 0.0, 0.0, 0.0]
                -   type: Tr2Vector4Parameter
                    name: "MaterialGlowColor"
                    value: [0.0, 0.0, 0.0, 0.0]
                resources:
                -   type: TriTextureParameter
                    name: "NormalMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_n_lowdetail.dds"
                -   type: TriTextureParameter
                    name: "PgsMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_pgs_lowdetail.dds"
                -   type: TriTextureParameter
                    name: "DiffuseMap"
                    resourcePath: "res:/dx9/model/station/gallente/gs5/gs5_d_lowdetail.dds"
shadowEffect:
    type: Tr2Effect
    effectFilePath: "res:/Graphics/Effect/Managed/Space/SpaceObject/Shadow/Shadow.fx"
damageLocators:
    structure: [["position", 68, 0], ["impactDirection", 100, 12]]
    items:
    - [[-6147.730957, 4963.4594727, -10178.6064453], [-0.1494735, 0.0, 0.0767277,
            0.9857842]]
    - [[-9230.9228516, -2134.3215332, -7461.7773438], [-0.1516642, 0.0, 0.9792034,
            0.1347547]]
    - [[7711.6054688, 5003.277832, 6012.9765625], [-0.2743862, 0.0, -0.1384226, 0.9516047]]
    - [[14606.9589844, -1941.0820313, -8317.5957031], [0.8237254, 0.0, -0.3431676,
            0.451345]]
    - [[12065.6953125, -1117.0878906, 9864.3447266], [-0.1451238, 0.0, -0.875287,
            0.461315]]
    - [[10070.4238281, 1869.3491211, 5988.2094727], [-0.2755793, 0.0, -0.2760497,
            0.9207891]]
    - [[8531.7265625, -1305.6861572, 831.7524414], [0.4616391, 0.0, -0.7805228, 0.4215131]]
    - [[7854.671875, 1024.0366211, -3002.9729004], [0.0015633, 0.0, 0.0, 0.9999987]]
    - [[13485.0195313, 1858.6156006, 799.9991455], [0.3507062, 0.0, -0.0089653, 0.9364426]]
    - [[-101.0688324, -1696.5496826, -3849.0402832], [0.5536576, 0.0, -0.7672908,
            0.3236172]]
    - [[4913.21875, -1769.1928711, 5841.828125], [-0.9855233, 0.0, 0.1553827, 0.0678237]]
    - [[12378.2587891, 1725.75, 10777.4902344], [-0.0052255, 0.0, -0.7070874, 0.7071068]]
    - [[-334.2174072, 4603.1669922, 12020.625], [0.0730291, -0.0, 0.4517708, 0.88914]]
    - [[-2049.9260254, 5276.9482422, -6753.9750977], [0.0188385, 0.0, -0.1214852,
            0.9924145]]
    - [[-1185.3812256, -2356.8908691, -6819.2285156], [-0.0033032, 0.0, -0.9997995,
            0.0197482]]
    - [[15621.6386719, 1809.0875244, -9851.8974609], [-0.0330756, 0.0, -0.4018143,
            0.9151236]]
    - [[-6158.4487305, 4715.6821289, -3126.3903809], [0.1984253, -0.0, 0.1003681,
            0.9749634]]
    - [[3328.1728516, -1539.8482666, 16566.2910156], [0.9434222, -0.0, 0.3064347,
            0.1266972]]
    - [[3510.8173828, -2989.251709, 13210.3525391], [0.6115463, -0.0, 0.7196413, 0.3288275]]
    - [[12190.328125, -753.480835, 12630.0078125], [0.2506223, 0.0, -0.7911571, 0.5579059]]
    - [[5913.8725586, -9267.0039063, 12740.6396484], [0.7407655, 0.0, -0.0572193,
            0.6693223]]
    - [[8082.6875, -3354.8071289, 10356.0058594], [-0.3285337, 0.0, -0.801098, 0.5003076]]
    - [[15494.7617188, -1702.9014893, -279.4299316], [0.26531, 0.0, -0.8937064, 0.3618001]]
    - [[-709.2335205, -1592.7580566, -11002.5185547], [-0.6712061, 0.0, -0.6668941,
            0.3236273]]
    - [[-5994.4697266, 430.7185974, 3181.458252], [-0.2815009, 0.0, -0.3484687, 0.8940508]]
    - [[-8685.2519531, 4720.887207, -6565.8232422], [0.0355723, -0.0, 0.2195636, 0.9749494]]
    - [[4340.1152344, -988.2861328, 86.1084366], [0.0819276, -0.0, 0.8629833, 0.4985456]]
    - [[-1948.1097412, 4887.8876953, -9731.1201172], [-0.1578079, 0.0, -0.1566546,
            0.9749646]]
    - [[7179.578125, -2817.3969727, 13589.2285156], [0.6178007, 0.0, -0.5379335, 0.5735416]]
    - [[10132.2675781, -1706.0643311, 7799.1972656], [-0.5857412, 0.0, -0.8076537,
            0.0678421]]
    - [[20544.2558594, -682.4985962, -12279.5703125], [0.3608211, 0.0, -0.5952443,
            0.7179779]]
    - [[-6443.0810547, -2412.7827148, -7417.269043], [-0.2273167, 0.0, 0.9736146,
            0.0200359]]
    - [[9191.9794922, 773.9961548, -7574.953125], [-0.1113231, 0.0, 0.0271248, 0.993414]]
    - [[-2857.3325195, 3424.0622559, -12122.5878906], [-0.5694095, 0.0, -0.1850249,
            0.8009611]]
    - [[1470.3605957, 5030.7138672, 14494.4667969], [0.187108, -0.0, 0.2172521, 0.9580147]]
    - [[5233.2719727, 2807.784668, -749.168457], [-0.0597046, 0.0, 0.26772, 0.9616451]]
    - [[20283.7519531, -4827.7905273, -13106.5693359], [-0.194624, 0.0, -0.822256,
            0.5348051]]
    - [[14012.3857422, 1078.5618896, -3164.9782715], [-0.3202366, 0.0, -0.0951065,
            0.9425514]]
    - [[10248.0146484, -1845.2359619, -3515.2329102], [0.9999999, 0.0, 0.0, -0.0]]
    - [[7732.328125, -827.1057129, -9620.1103516], [0.0, 0.0, 0.0, 1.0]]
    - [[-5224.8466797, -1609.0185547, -354.9879456], [0.9780311, -0.0, 0.0960652,
            0.1850044]]
    - [[10122.6103516, -2076.7207031, -6524.3364258], [-0.9667488, 0.0, 0.2355564,
            0.0995487]]
spriteSets:
-   type: EveSpriteSet
    sprites:
    -   type: EveSpriteSetItem
        name: "Glow"
        position: [7746.3330078, 56.7255936, 13670.4570313]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow1"
        position: [4973.5634766, 56.7255936, 13754.4501953]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow2"
        position: [6628.6455078, -1845.7550049, -222.869873]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow3"
        position: [-5314.3500977, -1844.0627441, 4163.4829102]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow4"
        position: [-4553.8583984, -2260.8496094, -6969.6040039]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow5"
        position: [19758.9785156, -898.2674561, -13317.7783203]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow6"
        position: [-4553.8583984, -1710.6665039, -6969.6040039]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow7"
        position: [-4553.8583984, -1023.7009888, -6969.6040039]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Glow8"
        position: [6628.6455078, -2035.5634766, -222.869873]
        blinkRate: 0.0
        minScale: 8000.0
        maxScale: 8000.0
        color: [0.0585, 0.078, 0.0728, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary1"
        position: [8435.0498047, -6806.0644531, -4220.4169922]
        blinkRate: 0.3
        blinkPhase: 0.16
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary1"
        position: [-8335.4150391, 8129.7177734, -3404.6774902]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary2"
        position: [-8335.4150391, 7805.1137695, -3404.6774902]
        blinkRate: 0.3
        blinkPhase: 0.39
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary3"
        position: [-8335.4150391, 7074.2661133, -3404.6774902]
        blinkRate: 0.3
        blinkPhase: 0.37
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary4"
        position: [-8335.4150391, 7634.390625, -3404.6774902]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary5"
        position: [-8062.8173828, 9014.2490234, -3132.9873047]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary6"
        position: [-8062.8173828, 8690.9013672, -3132.9873047]
        blinkRate: 0.3
        blinkPhase: 0.39
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary7"
        position: [-8062.8173828, 8375.0927734, -3132.9873047]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary8"
        position: [-8062.8173828, 7654.9819336, -3132.9873047]
        blinkRate: 0.3
        blinkPhase: 0.37
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary9"
        position: [-6844.1914063, 7192.8740234, -11532.8164063]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary10"
        position: [-6844.1914063, 7006.5551758, -11532.8164063]
        blinkRate: 0.3
        blinkPhase: 0.37
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary11"
        position: [-6844.1914063, 6737.0952148, -11532.8164063]
        blinkRate: 0.3
        blinkPhase: 0.36
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary12"
        position: [-6844.1914063, 6417.6430664, -11532.8164063]
        blinkRate: 0.3
        blinkPhase: 0.35
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary2"
        position: [5522.5766602, -21940.4101563, 10980.8134766]
        blinkRate: 0.5
        blinkPhase: 0.5
        minScale: 250.0
        maxScale: 850.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary3"
        position: [-4516.3652344, -9738.6582031, -6822.8549805]
        blinkRate: 0.3
        blinkPhase: 0.2
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary4"
        position: [-4787.4848633, -10560.4707031, -6970.8632813]
        blinkRate: 0.3
        blinkPhase: 0.18
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary5"
        position: [-6701.3134766, -469.2200928, -19945.3164063]
        blinkRate: 0.3
        blinkPhase: 0.47
        minScale: 130.0
        maxScale: 550.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary6"
        position: [3776.0380859, -469.2203369, -11260.8037109]
        blinkRate: 0.3
        blinkPhase: 0.5
        minScale: 130.0
        maxScale: 550.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary7"
        position: [-12889.0585938, -469.2203369, -2698.3869629]
        blinkRate: 0.3
        blinkPhase: 0.44
        minScale: 130.0
        maxScale: 550.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary8"
        position: [2090.043457, -469.2203369, -377.9689941]
        blinkRate: 0.3
        blinkPhase: 0.53
        minScale: 130.0
        maxScale: 550.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary9"
        position: [7816.75, -1350.9387207, -17720.1191406]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 130.0
        maxScale: 450.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary10"
        position: [8364.1777344, -1350.9387207, -16063.8759766]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 130.0
        maxScale: 450.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary11"
        position: [16937.1601563, 132.0921631, -4073.7548828]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary12"
        position: [18313.2109375, -255.4703369, -2572.5900879]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary13"
        position: [8697.3789063, 5556.3505859, 998.9362793]
        blinkRate: 0.3
        blinkPhase: 0.71
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary14"
        position: [5406.4296875, 6537.1396484, 1845.9626465]
        blinkRate: 0.3
        blinkPhase: 0.66
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary15"
        position: [12031.8525391, 4567.1230469, -6525.0947266]
        blinkRate: 0.3
        blinkPhase: 0.8
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary16"
        position: [11890.5957031, 3993.3378906, -6584.4677734]
        blinkRate: 0.3
        blinkPhase: 0.77
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary17"
        position: [7850.8652344, 4680.9589844, -2292.2822266]
        blinkRate: 0.3
        blinkPhase: 0.74
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary18"
        position: [7212.6835938, 7448.2490234, 9182.9814453]
        blinkRate: 0.3
        blinkPhase: 0.58
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary19"
        position: [4058.7263184, 11804.8115234, 9022.8232422]
        blinkRate: 0.3
        blinkPhase: 0.7
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary20"
        position: [3404.4384766, 9579.8427734, 9555.09375]
        blinkRate: 0.3
        blinkPhase: 0.67
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary21"
        position: [4559.7797852, 5188.0537109, -1445.2226563]
        blinkRate: 0.3
        blinkPhase: 0.68
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary22"
        position: [3199.8405762, 7984.171875, 11955.5107422]
        blinkRate: 0.3
        blinkPhase: 0.64
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary23"
        position: [6951.1171875, 6691.8271484, 12874.4599609]
        blinkRate: 0.3
        blinkPhase: 0.61
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary24"
        position: [5522.5766602, -21487.9667969, 10980.8134766]
        blinkRate: 0.5
        blinkPhase: 0.4
        minScale: 230.0
        maxScale: 550.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary13"
        position: [4058.7263184, 10935.6601563, 9022.8232422]
        blinkRate: 0.3
        blinkPhase: 0.65
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary14"
        position: [4058.7263184, 10397.8486328, 9022.8232422]
        blinkRate: 0.3
        blinkPhase: 0.63
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary15"
        position: [-10198.3642578, -425.7669678, 1881.5098877]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary16"
        position: [-1075.590332, -120.1890869, 1894.5998535]
        blinkRate: 0.3
        blinkPhase: 0.37
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary17"
        position: [-13810.1015625, -469.2203369, -8386.4550781]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary18"
        position: [-11213.7607422, -469.2203369, -13560.3798828]
        blinkRate: 0.3
        blinkPhase: 0.36
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary19"
        position: [-342.4243164, -469.2203369, -15343.0078125]
        blinkRate: 0.3
        blinkPhase: 0.34
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary20"
        position: [10582.6630859, 128.4412842, 19880.0996094]
        blinkRate: 0.3
        blinkPhase: 0.34
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary21"
        position: [6750.5561523, 273.4202881, 20259.8691406]
        blinkRate: 0.3
        blinkPhase: 0.32
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary22"
        position: [10314.3828125, 138.030426, 19364.3886719]
        blinkRate: 0.3
        blinkPhase: 0.3
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary23"
        position: [6623.9047852, 277.841095, 19755.9804688]
        blinkRate: 0.3
        blinkPhase: 0.28
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary24"
        position: [10116.515625, -13.6656494, 18977.7167969]
        blinkRate: 0.3
        blinkPhase: 0.26
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary25"
        position: [6827.5634766, -623.2372437, 17800.9414063]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 100.0
        maxScale: 450.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary26"
        position: [6678.6386719, -611.9125366, 17831.3984375]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 100.0
        maxScale: 450.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Secondary25"
        position: [-4561.5419922, -8951.7207031, -7116.3793945]
        blinkRate: 0.3
        blinkPhase: 0.16
        minScale: 130.0
        maxScale: 350.0
        color: [0.404, 0.1481333, 0.0, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary27"
        position: [7300.7387695, 1095.2006836, 17650.5957031]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary28"
        position: [7300.7387695, 1095.2006836, 17650.5957031]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary29"
        position: [7591.96875, 1050.8994141, 17555.8867188]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 850.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary30"
        position: [7663.4736328, 1071.2174072, 17535.7070313]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.559579, 0.652, 0.44662, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary31"
        position: [9277.671875, 1309.7692871, 16632.53125]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.1830014, 0.3309377, 0.3259937, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary32"
        position: [5844.9726563, 116.700592, 18193.9472656]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary33"
        position: [7707.9448242, -350.2287598, 17903.4433594]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.1710689, 0.3093461, 0.3047379, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary34"
        position: [7893.9638672, -350.1933289, 17837.5625]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.1360494, 0.2230259, 0.1838865, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary35"
        position: [4961.6235352, 1095.2006836, 17882.015625]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary36"
        position: [6303.7944336, 1540.6368408, 17738.4140625]
        blinkRate: 0.0
        minScale: 200.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary37"
        position: [1563.8172607, 439.3007202, 18173.5605469]
        blinkRate: 0.4
        minScale: 100.0
        maxScale: 300.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary38"
        position: [5247.9575195, -7628.3701172, 11776.2441406]
        blinkRate: 0.2
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary39"
        position: [1672.1921387, 1774.046875, 17350.8125]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary40"
        position: [18047.6757813, -63.6733971, 11673.1210938]
        blinkRate: 0.5
        blinkPhase: 0.5
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary41"
        position: [16216.9951172, -74.5037766, 4184.9833984]
        blinkRate: 0.5
        blinkPhase: 0.43
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary42"
        position: [6588.965332, 5659.5356445, 11649.8828125]
        blinkRate: 0.4
        blinkPhase: 0.4
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary43"
        position: [6164.0585938, 4428.6645508, 17014.9492188]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary44"
        position: [8269.34375, 4386.2094727, 16412.9648438]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary45"
        position: [8848.625, 5149.7685547, 14935.4365234]
        blinkRate: 0.0
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary46"
        position: [8507.4736328, 1567.2675781, 17081.4375]
        blinkRate: 0.0
        minScale: 200.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary47"
        position: [9241.7060547, 1517.5566406, 16670.0957031]
        blinkRate: 0.0
        minScale: 200.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary48"
        position: [1060.7145996, -531.9801636, 17862.4023438]
        blinkRate: 0.3
        minScale: 100.0
        maxScale: 300.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary49"
        position: [-2359.3701172, -328.9995728, 13262.3056641]
        blinkRate: 0.2
        minScale: 100.0
        maxScale: 300.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary50"
        position: [-2358.2658691, -215.1887665, 13262.3056641]
        blinkRate: 0.35
        blinkPhase: 0.2
        minScale: 100.0
        maxScale: 300.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary51"
        position: [-2499.4196777, -530.6057129, 12682.8623047]
        blinkRate: 0.4
        minScale: 100.0
        maxScale: 300.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary52"
        position: [-2499.4196777, 543.8503418, 12712.0732422]
        blinkRate: 0.55
        minScale: 100.0
        maxScale: 300.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary53"
        position: [6058.7612305, -7668.7246094, 11651.1982422]
        blinkRate: 0.6
        blinkPhase: 0.46
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary54"
        position: [6382.5395508, -7600.9511719, 10809.5273438]
        blinkRate: 0.4
        blinkPhase: 0.2
        minScale: 100.0
        maxScale: 450.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary55"
        position: [5948.1835938, -7097.9511719, 10245.1738281]
        blinkRate: 0.4
        minScale: 100.0
        maxScale: 450.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary56"
        position: [4787.4321289, -7570.7211914, 10544.28125]
        blinkRate: 0.4
        blinkPhase: 0.36
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary57"
        position: [6058.7612305, -7495.7255859, 11651.1982422]
        blinkRate: 0.6
        blinkPhase: 0.5
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary58"
        position: [4787.4321289, -7643.534668, 10544.28125]
        blinkRate: 0.4
        blinkPhase: 0.4
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary59"
        position: [18079.9199219, -63.6733971, 11674.7675781]
        blinkRate: 0.5
        blinkPhase: 0.46
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary60"
        position: [16259.1640625, -74.5037766, 4159.953125]
        blinkRate: 0.5
        blinkPhase: 0.38
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary61"
        position: [5373.7900391, 5659.5356445, 11621.5019531]
        blinkRate: 0.4
        blinkPhase: 0.38
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary62"
        position: [4353.2397461, 5659.5356445, 10158.3017578]
        blinkRate: 0.4
        blinkPhase: 0.36
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary63"
        position: [4724.8891602, 5659.5356445, 9790.9873047]
        blinkRate: 0.4
        blinkPhase: 0.34
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary64"
        position: [6178.0029297, 5659.5356445, 9319.65625]
        blinkRate: 0.4
        blinkPhase: 0.32
        minScale: 100.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary65"
        position: [1697.1883545, -1805.8950195, 7548.7871094]
        blinkRate: 0.0
        blinkPhase: 0.4
        minScale: 20.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary66"
        position: [2044.291626, -2081.911377, 13501.3515625]
        blinkRate: 0.0
        blinkPhase: 0.4
        minScale: 30.0
        maxScale: 200.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary67"
        position: [3730.9785156, -1777.6398926, 15790.7724609]
        blinkRate: 0.2
        blinkPhase: 0.4
        minScale: 200.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary68"
        position: [9777.3974609, -2068.0214844, 11493.7392578]
        blinkRate: 0.0
        blinkPhase: 0.4
        minScale: 20.0
        maxScale: 650.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary69"
        position: [12955.2910156, -2375.1669922, -182.0843811]
        blinkRate: 0.0
        blinkPhase: 0.4
        minScale: 20.0
        maxScale: 200.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary70"
        position: [14399.3515625, -2378.1101074, -707.3588257]
        blinkRate: 0.2
        blinkPhase: 0.4
        minScale: 60.0
        maxScale: 220.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary71"
        position: [13011.1035156, -2439.9770508, -1191.9436035]
        blinkRate: 0.0
        blinkPhase: 0.4
        minScale: 20.0
        maxScale: 120.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary72"
        position: [-5242.2792969, -2684.1306152, -5300.3696289]
        blinkRate: 0.4
        blinkPhase: 0.4
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary73"
        position: [-2764.8491211, -1016.0562134, 5911.3671875]
        blinkRate: 0.4
        blinkPhase: 0.4
        minScale: 120.0
        maxScale: 450.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary74"
        position: [15495.0175781, -416.5596008, 4625.3959961]
        blinkRate: 0.6
        blinkPhase: 0.4
        minScale: 60.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary75"
        position: [-6351.8876953, -2685.1889648, -7107.9711914]
        blinkRate: 0.4
        blinkPhase: 0.36
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary76"
        position: [-4978.9052734, -2685.1889648, -8723.5927734]
        blinkRate: 0.4
        blinkPhase: 0.32
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary77"
        position: [-3016.7285156, -2685.1889648, -7915.5009766]
        blinkRate: 0.4
        blinkPhase: 0.28
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary78"
        position: [-3179.2373047, -2685.1889648, -5800.1459961]
        blinkRate: 0.4
        blinkPhase: 0.24
        minScale: 100.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary79"
        position: [-2663.3781738, -1016.0562134, 5974.621582]
        blinkRate: 0.4
        blinkPhase: 0.36
        minScale: 120.0
        maxScale: 450.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary80"
        position: [-2523.0100098, -1016.0562134, 6062.8725586]
        blinkRate: 0.4
        blinkPhase: 0.32
        minScale: 120.0
        maxScale: 450.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary81"
        position: [17197.6386719, -417.3874817, 11605.7539063]
        blinkRate: 0.6
        blinkPhase: 0.4
        minScale: 60.0
        maxScale: 350.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary82"
        position: [19643.828125, -549.8023682, -13303.3964844]
        blinkRate: 0.2
        blinkPhase: 0.6
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary83"
        position: [19613.578125, -564.2175903, -13364.9091797]
        blinkRate: 0.2
        blinkPhase: 0.59
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary84"
        position: [19581.7539063, -591.7592773, -13419.0595703]
        blinkRate: 0.2
        blinkPhase: 0.58
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary85"
        position: [19553.8613281, -630.175293, -13470.1083984]
        blinkRate: 0.2
        blinkPhase: 0.57
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary86"
        position: [19531.0136719, -680.5770874, -13508.8652344]
        blinkRate: 0.2
        blinkPhase: 0.56
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary87"
        position: [19512.8085938, -735.8503418, -13546.3574219]
        blinkRate: 0.2
        blinkPhase: 0.56
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary88"
        position: [19501.453125, -802.4849243, -13565.2705078]
        blinkRate: 0.2
        blinkPhase: 0.54
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary89"
        position: [19495.6308594, -868.7318115, -13577.8457031]
        blinkRate: 0.2
        blinkPhase: 0.53
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary90"
        position: [19496.4941406, -935.2565308, -13575.9804688]
        blinkRate: 0.2
        blinkPhase: 0.52
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary91"
        position: [19501.8261719, -1002.746521, -13564.4658203]
        blinkRate: 0.2
        blinkPhase: 0.51
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary92"
        position: [19514.953125, -1067.8969727, -13538.7822266]
        blinkRate: 0.2
        blinkPhase: 0.5
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary93"
        position: [19533.4355469, -1124.567627, -13503.1142578]
        blinkRate: 0.2
        blinkPhase: 0.49
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary94"
        position: [19558.2617188, -1172.067627, -13458.5791016]
        blinkRate: 0.2
        blinkPhase: 0.48
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary95"
        position: [19585.5136719, -1211.0681152, -13409.8310547]
        blinkRate: 0.2
        blinkPhase: 0.47
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary96"
        position: [19617.90625, -1238.5588379, -13351.5595703]
        blinkRate: 0.2
        blinkPhase: 0.46
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary97"
        position: [19649.2480469, -1249.972168, -13293.2695313]
        blinkRate: 0.2
        blinkPhase: 0.45
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary98"
        position: [19682.6679688, -1249.972168, -13230.65625]
        blinkRate: 0.2
        blinkPhase: 0.44
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary99"
        position: [19715.1601563, -1233.5030518, -13171.4677734]
        blinkRate: 0.2
        blinkPhase: 0.43
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary100"
        position: [19746.0234375, -1205.3353271, -13113.6494141]
        blinkRate: 0.2
        blinkPhase: 0.42
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary101"
        position: [19772.9121094, -1167.4660645, -13066.1708984]
        blinkRate: 0.2
        blinkPhase: 0.41
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary102"
        position: [19795.921875, -1118.2160645, -13025.5400391]
        blinkRate: 0.2
        blinkPhase: 0.4
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary103"
        position: [19815.3808594, -1060.3728027, -12991.1826172]
        blinkRate: 0.2
        blinkPhase: 0.39
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary104"
        position: [19827.3769531, -996.4585571, -12969.9980469]
        blinkRate: 0.2
        blinkPhase: 0.38
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary105"
        position: [19832.2988281, -928.7243042, -12958.1533203]
        blinkRate: 0.2
        blinkPhase: 0.37
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary106"
        position: [19832.2988281, -860.5518799, -12958.1533203]
        blinkRate: 0.2
        blinkPhase: 0.36
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary107"
        position: [19824.9277344, -794.1201782, -12971.1699219]
        blinkRate: 0.2
        blinkPhase: 0.35
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary108"
        position: [19810.6582031, -726.947998, -12996.3632813]
        blinkRate: 0.2
        blinkPhase: 0.34
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary109"
        position: [19792.1601563, -670.1724854, -13029.0263672]
        blinkRate: 0.2
        blinkPhase: 0.33
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary110"
        position: [19768.2792969, -623.5370483, -13071.7636719]
        blinkRate: 0.2
        blinkPhase: 0.32
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary111"
        position: [19739.9628906, -584.4490356, -13124.078125]
        blinkRate: 0.2
        blinkPhase: 0.31
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary112"
        position: [19709.2128906, -558.0153198, -13180.1748047]
        blinkRate: 0.2
        blinkPhase: 0.3
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary113"
        position: [19676.3886719, -545.2763672, -13240.4169922]
        blinkRate: 0.2
        blinkPhase: 0.29
        minScale: 80.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary114"
        position: [6703.7744141, -220.604538, 18082.9316406]
        blinkRate: 0.3
        blinkPhase: 0.6
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary115"
        position: [6685.168457, -220.604538, 18022.7871094]
        blinkRate: 0.3
        blinkPhase: 0.59
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary116"
        position: [6664.7661133, -220.604538, 17964.875]
        blinkRate: 0.3
        blinkPhase: 0.58
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary117"
        position: [6646.362793, -220.604538, 17908.9511719]
        blinkRate: 0.3
        blinkPhase: 0.57
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary118"
        position: [6615.4575195, -220.604538, 17816.2207031]
        blinkRate: 0.3
        blinkPhase: 0.56
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary119"
        position: [6595.5483398, -222.046051, 17758.4902344]
        blinkRate: 0.3
        blinkPhase: 0.55
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary120"
        position: [6578.890625, -222.046051, 17700.1777344]
        blinkRate: 0.3
        blinkPhase: 0.54
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary121"
        position: [6557.9677734, -222.046051, 17640.765625]
        blinkRate: 0.3
        blinkPhase: 0.53
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary122"
        position: [6529.0673828, -222.046051, 17554.9648438]
        blinkRate: 0.3
        blinkPhase: 0.52
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary123"
        position: [6510.2763672, -222.046051, 17495.2929688]
        blinkRate: 0.3
        blinkPhase: 0.51
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary124"
        position: [6393.065918, -222.046051, 17291.2109375]
        blinkRate: 0.3
        blinkPhase: 0.5
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary125"
        position: [6368.8300781, -222.046051, 17222.125]
        blinkRate: 0.3
        blinkPhase: 0.49
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary126"
        position: [6346.9174805, -222.046051, 17156.9746094]
        blinkRate: 0.3
        blinkPhase: 0.48
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary127"
        position: [6322.2924805, -222.046051, 17090.5410156]
        blinkRate: 0.3
        blinkPhase: 0.47
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary128"
        position: [6286.2758789, -222.046051, 16984.1582031]
        blinkRate: 0.3
        blinkPhase: 0.46
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary129"
        position: [6262.2402344, -222.046051, 16917.0195313]
        blinkRate: 0.3
        blinkPhase: 0.45
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary130"
        position: [6239.487793, -222.046051, 16849.8046875]
        blinkRate: 0.3
        blinkPhase: 0.44
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary131"
        position: [6214.5053711, -222.046051, 16781.703125]
        blinkRate: 0.3
        blinkPhase: 0.43
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary132"
        position: [6181.9741211, -222.046051, 16683.7578125]
        blinkRate: 0.3
        blinkPhase: 0.42
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary133"
        position: [6161.074707, -222.046051, 16614.8339844]
        blinkRate: 0.3
        blinkPhase: 0.41
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary134"
        position: [5776.2407227, -222.046051, 15489.0771484]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary135"
        position: [5748.5844727, -222.046051, 15412.9697266]
        blinkRate: 0.3
        blinkPhase: 0.39
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary136"
        position: [5725.1171875, -222.046051, 15343.9228516]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary137"
        position: [5700.7910156, -222.046051, 15273.3730469]
        blinkRate: 0.3
        blinkPhase: 0.37
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary138"
        position: [5661.4799805, -222.046051, 15159.6591797]
        blinkRate: 0.3
        blinkPhase: 0.36
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary139"
        position: [5637.5683594, -222.046051, 15088.3398438]
        blinkRate: 0.3
        blinkPhase: 0.35
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary140"
        position: [5613.4111328, -222.046051, 15017.5976563]
        blinkRate: 0.3
        blinkPhase: 0.34
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary141"
        position: [5590.1455078, -222.046051, 14944.0244141]
        blinkRate: 0.3
        blinkPhase: 0.33
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary142"
        position: [5553.0205078, -222.046051, 14839.6376953]
        blinkRate: 0.3
        blinkPhase: 0.32
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary143"
        position: [5528.6972656, -222.046051, 14766.125]
        blinkRate: 0.3
        blinkPhase: 0.31
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary144"
        position: [6722.159668, 563.7020874, 18098.0957031]
        blinkRate: 0.3
        blinkPhase: 0.7
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary145"
        position: [6697.3310547, 560.385437, 18026.3300781]
        blinkRate: 0.3
        blinkPhase: 0.69
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary146"
        position: [6674.5195313, 560.385437, 17961.75]
        blinkRate: 0.3
        blinkPhase: 0.68
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary147"
        position: [6654.5869141, 560.385437, 17893.1972656]
        blinkRate: 0.3
        blinkPhase: 0.67
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary148"
        position: [6617.5112305, 560.385437, 17787.0742188]
        blinkRate: 0.3
        blinkPhase: 0.66
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary149"
        position: [6595.0673828, 561.0041504, 17716.6269531]
        blinkRate: 0.3
        blinkPhase: 0.65
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary150"
        position: [6572.9643555, 562.0189819, 17650.1875]
        blinkRate: 0.3
        blinkPhase: 0.64
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary151"
        position: [6550.4316406, 561.0322266, 17582.203125]
        blinkRate: 0.3
        blinkPhase: 0.63
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary152"
        position: [6516.9560547, 560.5010986, 17480.5566406]
        blinkRate: 0.3
        blinkPhase: 0.62
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary154"
        position: [6400.4301758, 561.5323486, 17309.5820313]
        blinkRate: 0.3
        blinkPhase: 0.6
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary155"
        position: [6371.9370117, 560.864563, 17228.3984375]
        blinkRate: 0.3
        blinkPhase: 0.59
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary156"
        position: [6344.8266602, 558.9439087, 17152.6484375]
        blinkRate: 0.3
        blinkPhase: 0.58
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary157"
        position: [6320.2558594, 559.9415894, 17072.953125]
        blinkRate: 0.3
        blinkPhase: 0.57
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary158"
        position: [6277.3154297, 561.399353, 16950.6640625]
        blinkRate: 0.3
        blinkPhase: 0.56
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary159"
        position: [6251.7739258, 561.8546753, 16871.8183594]
        blinkRate: 0.3
        blinkPhase: 0.55
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary160"
        position: [6225.0800781, 560.7227173, 16793.8808594]
        blinkRate: 0.3
        blinkPhase: 0.54
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary161"
        position: [6197.8696289, 560.5524902, 16714.7734375]
        blinkRate: 0.3
        blinkPhase: 0.53
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary162"
        position: [6158.3652344, 561.1378174, 16599.1523438]
        blinkRate: 0.3
        blinkPhase: 0.52
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary164"
        position: [5785.9624023, 560.038208, 15506.1630859]
        blinkRate: 0.3
        blinkPhase: 0.5
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary165"
        position: [5756.0488281, 560.0623779, 15419.4814453]
        blinkRate: 0.3
        blinkPhase: 0.49
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary166"
        position: [5728.4550781, 559.3164063, 15339.0039063]
        blinkRate: 0.3
        blinkPhase: 0.48
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary167"
        position: [5702.1870117, 561.1148682, 15255.5546875]
        blinkRate: 0.3
        blinkPhase: 0.47
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary168"
        position: [5655.7802734, 560.4399414, 15123.6347656]
        blinkRate: 0.3
        blinkPhase: 0.46
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary169"
        position: [5628.5302734, 561.0045776, 15040.0703125]
        blinkRate: 0.3
        blinkPhase: 0.45
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary170"
        position: [5600.8100586, 561.1604614, 14956.9785156]
        blinkRate: 0.3
        blinkPhase: 0.44
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary171"
        position: [5569.5507813, 559.9755859, 14871.8271484]
        blinkRate: 0.3
        blinkPhase: 0.43
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary172"
        position: [5527.3100586, 559.461792, 14750.296875]
        blinkRate: 0.3
        blinkPhase: 0.42
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary173"
        position: [8733.8271484, 567.6794434, 17418.3222656]
        blinkRate: 0.3
        blinkPhase: 0.7
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary174"
        position: [8712.7792969, 564.362793, 17352.8085938]
        blinkRate: 0.3
        blinkPhase: 0.69
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary175"
        position: [8691.5634766, 565.4711914, 17295.4570313]
        blinkRate: 0.3
        blinkPhase: 0.68
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary176"
        position: [8672.5810547, 564.362793, 17232.8789063]
        blinkRate: 0.3
        blinkPhase: 0.67
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary177"
        position: [8637.0683594, 564.5979004, 17140.2871094]
        blinkRate: 0.3
        blinkPhase: 0.66
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary178"
        position: [8617.0595703, 564.9815674, 17077.5332031]
        blinkRate: 0.3
        blinkPhase: 0.65
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary179"
        position: [8595.75, 565.9963989, 17016.0234375]
        blinkRate: 0.3
        blinkPhase: 0.64
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary180"
        position: [8573.1103516, 566.6885376, 16955.4199219]
        blinkRate: 0.3
        blinkPhase: 0.63
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary181"
        position: [8543.1386719, 565.644104, 16867.1835938]
        blinkRate: 0.3
        blinkPhase: 0.62
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary182"
        position: [8474.2841797, 567.8596802, 16526.1914063]
        blinkRate: 0.3
        blinkPhase: 0.6
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary183"
        position: [8451.9384766, 567.1918945, 16457.9902344]
        blinkRate: 0.3
        blinkPhase: 0.59
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary184"
        position: [8428.3984375, 565.2711792, 16388.5878906]
        blinkRate: 0.3
        blinkPhase: 0.58
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary185"
        position: [8391.1503906, 566.2688599, 16277.9570313]
        blinkRate: 0.3
        blinkPhase: 0.57
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary186"
        position: [8365.9902344, 567.7266846, 16206.0078125]
        blinkRate: 0.3
        blinkPhase: 0.56
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary187"
        position: [8342.109375, 568.1820068, 16137.8837891]
        blinkRate: 0.3
        blinkPhase: 0.55
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary188"
        position: [8318.2060547, 567.0500488, 16069.125]
        blinkRate: 0.3
        blinkPhase: 0.54
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary189"
        position: [8285.0908203, 566.8798218, 15965.0742188]
        blinkRate: 0.3
        blinkPhase: 0.53
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary190"
        position: [8257.7441406, 567.4651489, 15893.2597656]
        blinkRate: 0.3
        blinkPhase: 0.52
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary191"
        position: [7869.8222656, 565.697937, 14713.1904297]
        blinkRate: 0.3
        blinkPhase: 0.5
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary192"
        position: [7832.8759766, 564.4993286, 14603.3339844]
        blinkRate: 0.3
        blinkPhase: 0.49
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary193"
        position: [7806.6420898, 565.6437378, 14526.6435547]
        blinkRate: 0.3
        blinkPhase: 0.48
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary194"
        position: [7780.8354492, 565.3865967, 14455.1679688]
        blinkRate: 0.3
        blinkPhase: 0.47
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary195"
        position: [7757.0444336, 564.6045532, 14377.8808594]
        blinkRate: 0.3
        blinkPhase: 0.46
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary196"
        position: [7715.7460938, 564.5839844, 14259.9716797]
        blinkRate: 0.3
        blinkPhase: 0.45
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary197"
        position: [7691.5371094, 562.993042, 14184.1025391]
        blinkRate: 0.3
        blinkPhase: 0.44
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary198"
        position: [7665.7426758, 565.1370239, 14114.2646484]
        blinkRate: 0.3
        blinkPhase: 0.43
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary199"
        position: [7639.3662109, 565.7890625, 14032.9179688]
        blinkRate: 0.3
        blinkPhase: 0.42
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary200"
        position: [7897.0214844, 565.697937, 14789.6669922]
        blinkRate: 0.3
        blinkPhase: 0.51
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary201"
        position: [8499.9179688, 567.8596802, 16597.3457031]
        blinkRate: 0.3
        blinkPhase: 0.61
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary202"
        position: [8518.65625, 565.644104, 16806.1933594]
        blinkRate: 0.3
        blinkPhase: 0.61
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary203"
        position: [8735.7128906, -216.515213, 17333.96875]
        blinkRate: 0.3
        blinkPhase: 0.6
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary204"
        position: [8715.5097656, -216.515213, 17282.3066406]
        blinkRate: 0.3
        blinkPhase: 0.59
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary205"
        position: [8697.2246094, -216.515213, 17227.8554688]
        blinkRate: 0.3
        blinkPhase: 0.58
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary206"
        position: [8668.7792969, -216.515213, 17144.4042969]
        blinkRate: 0.3
        blinkPhase: 0.57
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary207"
        position: [8647.9912109, -216.515213, 17090.8554688]
        blinkRate: 0.3
        blinkPhase: 0.56
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary208"
        position: [8628.4501953, -217.9567413, 17038.6425781]
        blinkRate: 0.3
        blinkPhase: 0.55
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary209"
        position: [8611.0166016, -217.9567413, 16983.1308594]
        blinkRate: 0.3
        blinkPhase: 0.54
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary210"
        position: [8583.0810547, -217.9567413, 16905.8164063]
        blinkRate: 0.3
        blinkPhase: 0.53
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary211"
        position: [8564.3828125, -217.9567413, 16851.6152344]
        blinkRate: 0.3
        blinkPhase: 0.52
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary212"
        position: [8545.5898438, -217.9567413, 16800.7617188]
        blinkRate: 0.3
        blinkPhase: 0.51
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary213"
        position: [8506.5986328, -217.9567413, 16501.3554688]
        blinkRate: 0.3
        blinkPhase: 0.5
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary214"
        position: [8487.2167969, -217.9567413, 16442.5800781]
        blinkRate: 0.3
        blinkPhase: 0.49
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary215"
        position: [8464.2666016, -217.9567413, 16379.5390625]
        blinkRate: 0.3
        blinkPhase: 0.48
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary216"
        position: [8432.4189453, -217.9567413, 16283.2265625]
        blinkRate: 0.3
        blinkPhase: 0.47
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary217"
        position: [8410.2138672, -217.9567413, 16220.8623047]
        blinkRate: 0.3
        blinkPhase: 0.46
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary218"
        position: [8389.3066406, -217.9567413, 16159.5009766]
        blinkRate: 0.3
        blinkPhase: 0.45
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary219"
        position: [8368.6630859, -217.9567413, 16097.2382813]
        blinkRate: 0.3
        blinkPhase: 0.44
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary220"
        position: [8338.0576172, -217.9567413, 16007.5986328]
        blinkRate: 0.3
        blinkPhase: 0.43
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary221"
        position: [8316.9384766, -217.9567413, 15943.7138672]
        blinkRate: 0.3
        blinkPhase: 0.42
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary222"
        position: [8294.2822266, -217.9567413, 15882.0683594]
        blinkRate: 0.3
        blinkPhase: 0.41
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary223"
        position: [7900.6264648, -217.9567413, 14693.5224609]
        blinkRate: 0.3
        blinkPhase: 0.4
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary224"
        position: [7877.9462891, -217.9567413, 14628.7871094]
        blinkRate: 0.3
        blinkPhase: 0.39
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary225"
        position: [7856.6962891, -217.9567413, 14563.4736328]
        blinkRate: 0.3
        blinkPhase: 0.38
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary226"
        position: [7820.4174805, -217.9567413, 14459.2011719]
        blinkRate: 0.3
        blinkPhase: 0.37
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary227"
        position: [7796.5, -217.9567413, 14392.7929688]
        blinkRate: 0.3
        blinkPhase: 0.36
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary228"
        position: [7774.0566406, -217.9567413, 14327.2382813]
        blinkRate: 0.3
        blinkPhase: 0.35
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary229"
        position: [7752.3994141, -217.9567413, 14260.4296875]
        blinkRate: 0.3
        blinkPhase: 0.34
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary230"
        position: [7719.5532227, -217.9567413, 14164.9560547]
        blinkRate: 0.3
        blinkPhase: 0.33
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary231"
        position: [7696.3466797, -217.9567413, 14098.1425781]
        blinkRate: 0.3
        blinkPhase: 0.32
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary232"
        position: [7674.0024414, -217.9567413, 14032.3945313]
        blinkRate: 0.3
        blinkPhase: 0.31
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary233"
        position: [7921.3837891, -217.9567413, 14760.5820313]
        blinkRate: 0.3
        blinkPhase: 0.41
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary234"
        position: [8525.6630859, -217.9567413, 16565.2382813]
        blinkRate: 0.3
        blinkPhase: 0.51
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    -   type: EveSpriteSetItem
        name: "Primary235"
        position: [8753.9140625, -216.515213, 17389.4921875]
        blinkRate: 0.3
        blinkPhase: 0.61
        minScale: 70.0
        maxScale: 150.0
        color: [0.266546, 0.482, 0.4748182, 1.0]
    effect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/spaceobject/fx/blinkinglights.fx"
        resources:
        -   type: TriTextureParameter
            name: "GradientMap"
            resourcePath: "res:/texture/particle/whitesharp_gradient.dds"
spotlightSets:
-   type: EveSpotlightSet
    coneEffect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/spaceobject/fx/spotlightcone.fx"
        parameters:
        -   type: Tr2FloatParameter
            name: "zOffset"
            value: -0.02
        -   type: Tr2FloatParameter
            name: "MainIntensity"
        resources:
        -   type: TriTextureParameter
            name: "TextureMap"
            resourcePath: "res:/texture/global/spotramp.dds"
    glowEffect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/spaceobject/fx/spotlightglow.fx"
        resources:
        -   type: TriTextureParameter
            name: "TextureMap"
            resourcePath: "res:/texture/particle/whitesharp.dds"
    spotlightItems:
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_dockarea"
        transform: [428.8822327, -115.4368973, -86.0201721, 0.0, -70.6737518, -122.0669403,
            -188.5567932, 0.0, 188.305603, 1453.2684326, -1011.3892212, 0.0, 825.7427979,
            4246.4189453, 8466.4814453, 1.0]
        coneColor: [0.0823529, 0.1019608, 0.1019608, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "rimglow3"
        transform: [3479.114502, 74.6367569, -902.543335, 0.0, -7.3201728, 732.3407593,
            32.343956, 0.0, 550.3054199, -87.8665619, 2114.0454102, 0.0, 5771.5273438,
            -1325.059082, 11614.0625, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow20"
        transform: [3479.114502, 74.6367569, -902.543335, 0.0, -7.3201728, 732.3407593,
            32.343956, 0.0, 550.3054199, -87.8665619, 2114.0454102, 0.0, 5771.5273438,
            -1325.059082, 11614.0625, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow23"
        transform: [3479.114502, 74.6367569, -902.543335, 0.0, -7.3201728, 732.3407593,
            32.343956, 0.0, 550.3054199, -87.8665619, 2114.0454102, 0.0, 5771.5273438,
            -1325.059082, 11614.0625, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "towerglow33"
        transform: [-3167.5390625, -4.6536341, 2323.7668457, 0.0, 1096.1109619, 299.8252563,
            1494.71521, 0.0, -147.1557312, 1522.7667236, -197.5392303, 0.0, 9054.9257813,
            2358.4758301, 15877.9941406, 1.0]
        coneColor: [0.090506, 0.118, 0.1060859, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "bigdockingglow2"
        transform: [5627.7602539, 0.0, -1772.9777832, 0.0, 0.0, 1494.4251709, 0.0,
            0.0, 2424.8085938, 0.0, 7696.7919922, 0.0, 6264.6708984, -347.9983521,
            18179.9980469, 1.0]
        coneColor: [0.0235294, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.7254902, 0.9411765, 0.9098039, 1.0]
        flareColor: [0.4352941, 0.6235294, 0.9254902, 1.0]
        spriteScale: [4000.0, 3000.0, 350.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_dockarea2"
        transform: [428.8822327, -115.4368973, -86.0201721, 0.0, -70.6737518, -122.0669403,
            -188.5567932, 0.0, 188.305603, 1453.2684326, -1011.3892212, 0.0, 825.7427979,
            4246.4189453, 8466.4814453, 1.0]
        coneColor: [0.0823529, 0.1019608, 0.1019608, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "rimglow37"
        transform: [3479.114502, 74.6367569, -902.543335, 0.0, -7.3201728, 732.3407593,
            32.343956, 0.0, 550.3054199, -87.8665619, 2114.0454102, 0.0, 5771.5273438,
            -1325.059082, 11614.0625, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "towerglow46"
        transform: [152.1455231, 6.5240693, 641.7928467, 0.0, 1591.7609863, -19.8864822,
            -377.1459351, 0.0, 12.8316154, 1343.8347168, -16.7024975, 0.0, 8020.0541992,
            2919.5593262, -4115.3100586, 1.0]
        coneColor: [0.0220645, 0.0259098, 0.0287785, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "bigdockingglow3"
        transform: [5568.0400391, 0.0, -1952.4511719, 0.0, 0.0, 1494.4251709, 0.0,
            0.0, 2670.2648926, 0.0, 7615.1157227, 0.0, 9083.8652344, -381.8780823,
            17261.5234375, 1.0]
        coneColor: [0.0235294, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.7254902, 0.9411765, 0.9098039, 1.0]
        flareColor: [0.4352941, 0.6235294, 0.9254902, 1.0]
        spriteScale: [4000.0, 3000.0, 350.0]
    -   type: EveSpotlightSetItem
        name: "rimglow38"
        transform: [3479.114502, 74.6367569, -902.543335, 0.0, -7.3201728, 732.3407593,
            32.343956, 0.0, 550.3054199, -87.8665619, 2114.0454102, 0.0, 5771.5273438,
            -1325.059082, 11614.0625, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow39"
        transform: [-1845.2208252, 0.0, -3085.3828125, 0.0, 0.0, 1038.0666504, 0.0,
            0.0, 1876.3200684, 0.0, -1122.1378174, 0.0, 17666.9628906, -226.1885834,
            3284.8579102, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_dock4"
        transform: [-328.472229, 0.0, 0.0, 0.0, -0.0, -0.0, -170.9690247, 0.0, -0.0,
            -415.2300415, 0.0, 0.0, -5314.3500977, -1840.770874, 4163.4829102, 1.0]
        coneColor: [0.0823529, 0.1019608, 0.1019608, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazydock6"
        transform: [-1435.4276123, 0.0, 0.0, 0.0, -0.0, -0.0, -747.1292725, 0.0, -0.0,
            -3103.5625, 0.0, 0.0, 6619.128418, -1769.1942139, -229.8893127, 1.0]
        coneColor: [0.0823529, 0.1019608, 0.1019608, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [1200.0, 800.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "rimglow40"
        transform: [-851.7542114, 76.8060532, -1593.3747559, 0.0, -27.1490917, 519.9511108,
            39.5762177, 0.0, 968.4203491, 89.6401443, -513.3577271, 0.0, -6898.043457,
            765.0457764, 5065.746582, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow41"
        transform: [3479.114502, 74.6367569, -902.543335, 0.0, -7.3201728, 732.3407593,
            32.343956, 0.0, 550.3054199, -87.8665619, 2114.0454102, 0.0, 5771.5273438,
            -1325.059082, 11614.0625, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_bottomlarge13"
        transform: [1618.6490479, 0.0000538, -951.8600464, 0.0, 482.7119751, -220.1873016,
            820.8572998, 0.0, -303.8074036, -2592.0178223, -516.6281128, 0.0, 6759.9204102,
            -4092.8198242, 12751.2373047, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer24"
        transform: [-546.9078979, -883.9031372, -500.966217, 0.0, 417.8311768, -14.1509352,
            -431.1809692, 0.0, 2451.1523438, -2917.1086426, 2470.9987793, 0.0, -10408.7255859,
            -952.3637085, -12754.5625, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerglow47"
        transform: [-3124.307373, -67.2912903, -2380.6286621, 0.0, 1138.2508545, -28.9006729,
            -1493.0093994, 0.0, 6.6218328, -1542.1520996, 34.9003448, 0.0, -1457.263916,
            3314.8173828, -11190.4375, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight27"
        transform: [0.0, 726.4888306, 0.0, 0.0, -371.691864, 0.0, 69.5372849, 0.0,
            133.3013, -0.0, 712.524353, 0.0, -3600.8828125, 380.2410278, -923.6306763,
            1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazydock7"
        transform: [0.0, 0.0, -2992.4367676, 0.0, 1240.4589844, 0.0, 0.0, 0.0, 0.0,
            -6737.402832, 0.0, 0.0, -4552.8417969, -780.6212769, -7003.2539063, 1.0]
        coneColor: [0.0823529, 0.1019608, 0.1019608, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [1200.0, 800.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "rimglow42"
        transform: [322.5107727, 0.0, -3580.5605469, 0.0, 0.0, 1038.0666504, 0.0,
            0.0, 2177.4526367, 0.0, 196.1290588, 0.0, 19745.7578125, -221.3045197,
            11790.7607422, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer25"
        transform: [-73.911499, -1151.0881348, -30.2507038, 0.0, -378.1123352, 12.0197706,
            466.4696655, 0.0, -3516.3706055, 300.8967285, -2858.0632324, 0.0, -3661.9797363,
            -2601.6325684, -6200.7768555, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer26"
        transform: [269.5512085, -359.8607483, 1062.6394043, 0.0, 582.0231323, -1.5001616,
            -148.1451569, 0.0, 241.4644623, 2895.5695801, 919.3286133, 0.0, 8927.4003906,
            -820.3282471, -8936.8642578, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyglow_warm8"
        transform: [-2225.6079102, 0.0000177, 754.9382935, 0.0, -351.3659363, -1502.8150635,
            -1035.8499756, 0.0, 718.87677, -1628.8508301, 2119.2961426, 0.0, 4979.4379883,
            937.2935181, 11447.2558594, 1.0]
        coneColor: [0.2509804, 0.3294118, 0.3372549, 1.0]
        spriteColor: [0.7529412, 0.7960784, 0.6627451, 1.0]
        flareColor: [0.282353, 0.254902, 0.1882353, 1.0]
        spriteScale: [5000.0, 5000.0, 350.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer27"
        transform: [-738.3184814, -883.9032593, -70.4349289, 0.0, 73.7859802, -14.1506624,
            -595.8653564, 0.0, 3445.010498, -2917.1081543, 495.8712158, 0.0, -12685.2695313,
            -952.6947021, -8212.7939453, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer28"
        transform: [-134.5198517, -883.9031982, -729.3693848, 0.0, 593.2838745, -14.1507568,
            -92.2722549, 0.0, 466.8482666, -2917.1083984, 3449.0632324, 0.0, -5894.3803711,
            -952.6947021, -15090.1279297, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer29"
        transform: [316.657959, -883.9031982, -670.673584, 0.0, 535.5265503, -14.1508179,
            271.4982605, 0.0, -1634.8443604, -2917.1083984, 3072.6647949, 0.0, -860.8497925,
            -952.6947021, -14322.3554688, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyglow_warm9"
        transform: [-2225.6079102, 0.0000177, 754.9382935, 0.0, -351.3659363, -1502.8150635,
            -1035.8499756, 0.0, 718.87677, -1628.8508301, 2119.2961426, 0.0, 6160.9672852,
            937.2935181, 11046.4746094, 1.0]
        coneColor: [0.2509804, 0.3294118, 0.3372549, 1.0]
        spriteColor: [0.7529412, 0.7960784, 0.6627451, 1.0]
        flareColor: [0.282353, 0.254902, 0.1882353, 1.0]
        spriteScale: [5000.0, 5000.0, 350.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_bottomlarge14"
        transform: [1877.7380371, 0.0, 12.7313242, 0.0, -6.4563713, -220.1873627,
            952.2476807, 0.0, 4.0830207, -2604.4770508, -602.203186, 0.0, 5370.2929688,
            -4091.4814453, 13161.6083984, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_bottomlarge15"
        transform: [1048.06604, -0.0001304, 1558.0820313, 0.0, -790.1421509, -220.1874847,
            531.5003052, 0.0, 526.7788086, -2745.6821289, -354.345459, 0.0, 3781.6306152,
            -4091.4814453, 12235.4453125, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_bottomlarge16"
        transform: [-603.3606567, -0.0000065, 1778.206543, 0.0, -901.772644, -220.1870422,
            -305.9791565, 0.0, 633.1162109, -2891.440918, 214.8217316, 0.0, 3460.3718262,
            -4091.4814453, 10337.046875, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_bottomlarge17"
        transform: [-1846.7275391, 0.0000103, -340.0880737, 0.0, 172.4671173, -220.1873322,
            -936.5214233, 0.0, -130.8973541, -3125.7333984, 710.7914429, 0.0, 5852.9038086,
            -4091.4814453, 8847.9921875, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_bottomlarge18"
        transform: [-598.1030884, 0.000166, -1779.9818115, 0.0, 902.6730347, -220.1874084,
            -303.3129883, 0.0, -596.6317139, -2722.0944824, 200.4778137, 0.0, 7558.9775391,
            -4091.4814453, 10350.5048828, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "rimglow43"
        transform: [-782.196106, 0.0, -3508.9306641, 0.0, 0.0, 1038.0666504, 0.0,
            0.0, 2133.8930664, 0.0, -475.6784668, 0.0, 1391.793335, -1445.1274414,
            -8427.2402344, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow44"
        transform: [3531.8630371, 0.0, -671.0961304, 0.0, 0.0, 1038.0666504, 0.0,
            0.0, 408.1150513, 0.0, 2147.8388672, 0.0, -3101.7976074, -1445.1274414,
            -1020.7481079, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow45"
        transform: [770.2641602, 0.0, 3511.5700684, 0.0, -0.0, 1038.0666504, 0.0,
            0.0, -2135.4978027, -0.0, 468.4222107, 0.0, -10500.8544922, -1445.1274414,
            -5514.7978516, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow46"
        transform: [-3478.9338379, 0.0, 906.3362427, 0.0, 0.0, 1038.0666504, 0.0,
            0.0, -551.171936, 0.0, -2115.6503906, 0.0, -6006.8647461, -1445.1274414,
            -12912.28125, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer30"
        transform: [-73.9114532, -1151.0881348, -30.2506733, 0.0, -555.5471802, 41.56847,
            -224.3808746, 0.0, 1700.8286133, 1.4506189, -4210.8320313, 0.0, -5004.9570313,
            -2523.8273926, -5880.0913086, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer31"
        transform: [-73.9115448, -1151.0881348, -30.2507992, 0.0, 35.6735497, 13.4611444,
            -599.3775635, 0.0, 4523.9960938, -297.387085, 262.5787964, 0.0, -5729.3583984,
            -2446.2036133, -7063.8720703, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer32"
        transform: [-73.9115372, -1151.0881348, -30.2506733, 0.0, 578.3501587, -32.9697952,
            -158.5289612, 0.0, 1189.3066406, -191.4375305, 4378.6787109, 0.0, -4827.4140625,
            -2476.6325684, -8109.7275391, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer33"
        transform: [-73.9114456, -1151.0881348, -30.2506828, 0.0, 312.9122314, -33.5353088,
            511.535614, 0.0, -3865.3505859, 185.7357178, 2376.6560059, 0.0, -3552.8525391,
            -2572.6618652, -7569.7924805, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "rimglow47"
        transform: [-1813.7126465, -142.7959747, -207.8205414, 0.0, -43.8521423, 526.5007324,
            20.9459743, 0.0, 122.4072037, 54.176178, -1105.5097656, 0.0, -5433.2553711,
            773.7302246, 5956.4882813, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow48"
        transform: [-1572.0111084, -18.8693428, 1523.7635498, 0.0, 31.2598915, 630.1323853,
            40.0528564, 0.0, -924.3786011, 106.3895721, -952.3300781, 0.0, -3976.6184082,
            777.0217896, 5297.4135742, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow49"
        transform: [211.7105255, -179.2082825, 2111.791748, 0.0, 27.878149, 612.4051514,
            49.1743431, 0.0, -1287.5256348, 47.9202538, 133.1430664, 0.0, -3536.6196289,
            773.7302246, 4026.3239746, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow50"
        transform: [2106.2382813, -193.6775665, 1191.6668701, 0.0, 64.4287415, 698.0267334,
            -0.4281085, 0.0, -721.5511475, 67.3889618, 1286.2741699, 0.0, -4301.9111328,
            773.7302246, 2654.9919434, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow51"
        transform: [2380.9030762, -198.3672028, -425.8882446, 0.0, 51.1292381, 697.7666016,
            -39.1657906, 0.0, 264.6424255, 62.0293388, 1450.5761719, 0.0, -5634.046875,
            773.5857544, 2364.076416, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow52"
        transform: [787.7369995, -155.9245911, -1708.3112793, 0.0, -1.1812096, 542.7399292,
            -50.0827789, 0.0, 1043.1938477, 46.2697334, 476.8146057, 0.0, -6977.6875,
            773.3614502, 3398.7890625, 1.0]
        coneColor: [0.1176471, 0.1137255, 0.0901961, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight28"
        transform: [0.0, 726.4888306, 0.0, 0.0, -371.691864, 0.0, 69.5372849, 0.0,
            133.3013, -0.0, 712.524353, 0.0, -3613.3076172, 494.8538818, -990.0438232,
            1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight29"
        transform: [161.2445526, 646.3197021, -289.9259949, 0.0, -330.4698486, 0.000019,
            -183.7932892, 0.0, -313.4474792, 331.0164795, 563.5947876, 0.0, -7304.800293,
            494.8538818, -1582.1588135, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight30"
        transform: [167.0334015, 643.3442993, -293.2473145, 0.0, -328.5765381, -0.0000491,
            -187.1569977, 0.0, -317.7146912, 336.7376404, 557.7861328, 0.0, -7328.7148438,
            380.2410278, -1530.8942871, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight31"
        transform: [257.8802795, 666.1548462, -132.3686981, 0.0, -172.678299, 0.0000236,
            -336.4111938, 0.0, -591.3347778, 289.2290955, 303.5293884, 0.0, -9949.8554688,
            494.8538818, -4225.5273438, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight32"
        transform: [298.848114, 645.8364258, -146.1885986, 0.0, -166.1611938, -0.0000098,
            -339.6773987, 0.0, -578.864563, 331.9541931, 283.1652222, 0.0, -9997.4257813,
            380.2410278, -4200.6772461, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight33"
        transform: [-261.8390198, 626.7097778, -257.8007507, 0.0, -265.2998352, -0.0000207,
            269.4555359, 0.0, 445.5959778, 366.6413879, 438.7237549, 0.0, -282.1266785,
            494.8538818, -2686.6855469, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight34"
        transform: [-282.9154968, 617.5255737, -257.6956177, 0.0, -254.6350861, 0.0000111,
            279.5554504, 0.0, 455.5233459, 381.8414001, 414.916687, 0.0, -237.7322235,
            380.2410278, -2649.5925293, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight35"
        transform: [-380.4949341, 618.1309814, -30.3931904, 0.0, -30.1091976, 0.0000177,
            376.9399414, 0.0, 614.8092041, 380.8648987, 49.1096954, 0.0, 1425.40979,
            494.8538818, -6014.8847656, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight36"
        transform: [-328.0741272, 644.4903564, -69.1775818, 0.0, -78.0189819, 0.0000092,
            370.0044861, 0.0, 629.2325439, 334.548645, 132.6796875, 0.0, 1478.9766846,
            380.2410278, -6015.8227539, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight37"
        transform: [-276.244812, 656.0305786, 145.2539673, 0.0, 154.655426, -11.2390366,
            344.8849792, 0.0, 601.3239746, 310.671051, -259.5252686, 0.0, 837.5213013,
            495.2626648, -9712.0556641, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerspotlight38"
        transform: [-278.9242554, 656.946106, 135.6800537, 0.0, 165.4109192, 0.0000227,
            340.043396, 0.0, 589.456604, 309.4897156, -286.7356567, 0.0, 889.6846313,
            382.0927429, -9739.7636719, 1.0]
        coneColor: [0.0862745, 0.1058824, 0.1058824, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [400.0, 1500.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "towerglow48"
        transform: [-3928.5134277, 0.0, 0.0, 0.0, -0.0, -0.0, -1877.6387939, 0.0,
            -0.0, -1542.5611572, 0.0, 0.0, -4552.7700195, 3308.4958496, -12202.3769531,
            1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow49"
        transform: [-3133.1027832, 0.0, 2369.9968262, 0.0, -1132.7434082, 0.0, -1497.4710693,
            0.0, 0.0, -1542.5611572, 0.0, 0.0, -7619.5991211, 3310.6513672, -11209.2148438,
            1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow50"
        transform: [-3694.6657715, -0.0001631, -1335.1636963, 0.0, 638.1434326, 0.0000137,
            -1765.8709717, 0.0, 0.000064, -1542.5611572, 0.0000112, 0.0, -2916.8391113,
            3308.4958496, -11906.8867188, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow51"
        transform: [-3709.0458984, 0.0, 1294.6800537, 0.0, -618.7942505, -0.0, -1772.7438965,
            0.0, -0.0, -1542.5611572, 0.0, 0.0, -6133.6044922, 3310.6513672, -11948.5341797,
            1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow52"
        transform: [-2246.3203125, -0.0002936, 3222.9279785, 0.0, -1540.4030762, -0.0000732,
            -1073.6320801, 0.0, 0.0001153, -1542.5611572, -0.0000602, 0.0, -8755.8662109,
            3310.6513672, -10083.390625, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow53"
        transform: [-1317.9069824, 0.000215, 3700.8564453, 0.0, -1768.8300781, 0.0002662,
            -629.8956909, 0.0, -0.0002343, -1542.5611572, 0.0000062, 0.0, -9488.7363281,
            3310.651123, -8636.3466797, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow54"
        transform: [-1.5997654, -0.0007024, 3928.5144043, 0.0, -1877.6386719, 0.000112,
            -0.7646101, 0.0, -0.0000919, -1542.5611572, -0.0002759, 0.0, -9770.6376953,
            3310.651123, -7006.3657227, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow55"
        transform: [1207.5809326, 0.0009371, 3738.3105469, 0.0, -1786.7310791, -0.000326,
            577.1651001, 0.0, 0.000368, -1542.5611572, 0.0002678, 0.0, -9521.5732422,
            3310.6508789, -5391.2973633, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow56"
        transform: [2237.8068848, -0.0001536, 3228.8447266, 0.0, -1543.2310791, -0.0001402,
            1069.5631104, 0.0, 0.0000603, -1542.5611572, -0.0001152, 0.0, -8795.5107422,
            3310.6508789, -3936.2995605, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow57"
        transform: [3146.4924316, -0.0003134, 2352.1911621, 0.0, -1124.2331543, -0.0004506,
            1503.8707275, 0.0, 0.0001231, -1542.5611572, -0.0003702, 0.0, -7645.7348633,
            3310.6506348, -2769.5202637, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow58"
        transform: [3621.2460938, -0.0002776, 1523.0878906, 0.0, -727.9620361, -0.0004576,
            1730.7797852, 0.0, 0.0000453, -1542.5611572, -0.0003888, 0.0, -6215.1049805,
            3310.6506348, -2032.6043701, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow59"
        transform: [3928.3059082, -0.0001022, -40.3758698, 0.0, 19.2977104, -0.0000003,
            1877.5400391, 0.0, -0.0000401, -1542.5611572, 0.0000002, 0.0, -4634.8579102,
            3310.6506348, -1748.1955566, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow60"
        transform: [3749.5280762, 0.0, -1172.2867432, 0.0, 560.2961426, -0.0001639,
            1792.0925293, 0.0, -0.0000402, -1542.5611572, -0.0001285, 0.0, -3023.5766602,
            3310.6503906, -1983.706665, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow61"
        transform: [3227.2260742, -0.0001381, -2240.1408691, 0.0, 1070.6785889, -0.0001865,
            1542.4573975, 0.0, -0.0001319, -1542.5611572, -0.000095, 0.0, -1538.9760742,
            3310.6506348, -2704.1877441, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow62"
        transform: [2255.5124512, -0.0002938, -3216.5014648, 0.0, 1537.331665, -0.000073,
            1078.0255127, 0.0, -0.0001154, -1542.5611572, 0.00006, 0.0, -375.29245,
            3310.6506348, -3838.0869141, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow63"
        transform: [1188.7374268, -0.0014796, -3744.3452148, 0.0, 1789.6153564, -0.0009097,
            568.1588135, 0.0, -0.0008881, -1542.5611572, 0.0003276, 0.0, 387.5102539,
            3310.6506348, -5276.347168, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow64"
        transform: [4.7014165, -0.0201507, -3928.510498, 0.0, 1877.6374512, -0.0105143,
            2.2470489, 0.0, -0.0086474, -1542.5611572, 0.007902, 0.0, 669.3984985,
            3310.6506348, -6892.0512695, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow65"
        transform: [-1038.2202148, 0.0010531, -3788.8410645, 0.0, 1810.8822021, -0.000384,
            -496.2189026, 0.0, -0.0004135, -1542.5611572, -0.0003155, 0.0, 440.269104,
            3310.6508789, -8501.2890625, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow66"
        transform: [-2350.4199219, 0.0014802, -3147.8156738, 0.0, 1504.5032959, -0.0003547,
            -1123.3868408, 0.0, -0.0005812, -1542.5611572, -0.0002914, 0.0, -276.6598511,
            3310.651123, -9970.2177734, 1.0]
        coneColor: [0.021476, 0.028, 0.0250642, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer34"
        transform: [274.4441223, -295.0181274, 1081.2055664, 0.0, 582.0231323, -1.5000796,
            -148.1451721, 0.0, 196.7986908, 2908.7072754, 743.7171021, 0.0, 7985.2861328,
            -817.9000244, -8697.0634766, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "hazyspotlight_outer35"
        transform: [270.6094666, -346.9227905, 1066.6655273, 0.0, 582.0231323, -1.5000848,
            -148.1452026, 0.0, 159.8144379, 1993.083252, 607.6869507, 0.0, 7026.9033203,
            -900.5252075, -8462.8681641, 1.0]
        coneColor: [0.0509804, 0.0627451, 0.0627451, 1.0]
        spriteColor: [0.6862745, 0.8901961, 0.8666667, 1.0]
        flareColor: [0.4078431, 0.6039216, 0.9254902, 1.0]
        spriteScale: [800.0, 2000.0, 150.0]
    -   type: EveSpotlightSetItem
        name: "rimglow53"
        transform: [-782.1976318, -0.0006418, -3508.9304199, 0.0, -1012.5546875, -36.9912033,
            225.71492, 0.0, -27.4991093, 790.1325073, 6.1298547, 0.0, 13903.4902344,
            2837.4555664, -9928.2041016, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "rimglow54"
        transform: [-899.6707764, -18.336031, -1672.4995117, 0.0, 98.5383759, -1567.9243164,
            -35.8162041, 0.0, -416.6817627, -31.3148632, 224.484726, 0.0, 17945.25,
            -876.6688232, -12332.0917969, 1.0]
        coneColor: [0.0705882, 0.0705882, 0.054902, 1.0]
        spriteColor: [0.8901961, 0.8588235, 0.6862745, 1.0]
        flareColor: [0.8901961, 0.8235294, 0.6862745, 1.0]
        spriteScale: [600.0, 100.0, 50.0]
    -   type: EveSpotlightSetItem
        name: "towerglow67"
        transform: [-2828.8728027, -4.6533604, -2725.9267578, 0.0, -293.3720703, 68.377121,
            304.3347168, 0.0, 169.6179657, 1522.7667236, -178.6231689, 0.0, 1342.9007568,
            2362.2578125, 15295.4423828, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow68"
        transform: [-2136.0109863, -27.6295719, -3296.9545898, 0.0, -364.0794678,
            53.9558563, 235.425415, 0.0, 154.0266724, 1530.7148438, -112.6177597,
            0.0, 313.7989502, 2346.4287109, 13973.8603516, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow69"
        transform: [-3730.7282715, -4.6539164, -1230.8007813, 0.0, -186.4473572, 96.2075043,
            564.7840576, 0.0, 75.4586182, 1522.7667236, -234.4834595, 0.0, 3832.1467285,
            2358.4758301, 16773.8398438, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow70"
        transform: [-3920.0932617, -4.6537147, -257.0292358, 0.0, -24.1203823, 59.4602699,
            366.7965393, 0.0, 14.3158455, 1522.7667236, -245.9096527, 0.0, 5523.4628906,
            2360.8840332, 16958.3945313, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow71"
        transform: [-3708.0715332, -4.6536789, 1297.4594727, 0.0, 179.90802, 88.1631699,
            514.4839478, 0.0, -83.0539398, 1522.7667236, -231.9020233, 0.0, 7411.4116211,
            2360.8840332, 16690.5527344, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow72"
        transform: [-1115.8444824, -4.6532702, 3766.7077637, 0.0, 545.6851196, 92.0655746,
            161.7667694, 0.0, -236.6865234, 1522.7667236, -68.2345276, 0.0, 11279.9580078,
            2348.9538574, 12768.0478516, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow73"
        transform: [-2428.7932129, -4.6536956, 3087.7434082, 0.0, 499.5035706, 102.8147812,
            393.0602722, 0.0, -194.7182159, 1522.7667236, -150.8686676, 0.0, 10416.0878906,
            2351.9335938, 14532.2695313, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow75"
        transform: [218.7551575, 4.5318918, 3922.4157715, 0.0, 413.8153381, 67.0425797,
            -23.1561565, 0.0, -246.0353851, 1522.7667236, 11.962142, 0.0, 11555.5380859,
            2333.6208496, 10380.6474609, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow76"
        transform: [1393.9725342, -4.6537852, 3672.8789063, 0.0, 840.1400757, 145.3470459,
            -318.675293, 0.0, -229.6511078, 1522.7667236, 89.0892334, 0.0, 11099.5410156,
            2349.8933105, 8620.4902344, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow77"
        transform: [2239.260498, -4.6537743, 3227.8334961, 0.0, 688.8058472, 135.5877533,
            -477.6531067, 0.0, -201.3583527, 1522.7667236, 141.8847656, 0.0, 10135.4003906,
            2339.4277344, 7078.3710938, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow78"
        transform: [3487.4194336, -4.6539092, 1808.6199951, 0.0, 264.1134949, 92.7630463,
            -509.0305176, 0.0, -111.7997665, 1522.7667236, 219.4930267, 0.0, 8681.5117188,
            2341.2634277, 5819.0112305, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow79"
        transform: [3861.6616211, -4.6539006, 721.6407471, 0.0, 109.1093292, 95.9816742,
            -583.2495117, 0.0, -43.4739571, 1522.7667236, 242.4593048, 0.0, 6903.1308594,
            2345.9250488, 5062.4824219, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow80"
        transform: [3902.8874512, -4.6536846, -447.9666138, 0.0, -60.9021301, 86.5377426,
            -531.5058594, 0.0, 29.8798199, 1522.7667236, 244.5072784, 0.0, 5025.8447266,
            2351.2260742, 4945.1333008, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow81"
        transform: [3498.3374023, -4.6534839, -1787.4090576, 0.0, -192.6676483, 68.5236282,
            -377.2697449, 0.0, 113.6776352, 1522.7667236, 218.5266724, 0.0, 3132.6020508,
            2345.1020508, 5403.3076172, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow82"
        transform: [2743.8605957, -4.654232, -2811.4807129, 0.0, -286.9206848, 64.8634644,
            -280.1271973, 0.0, 177.5410156, 1522.7667236, 170.7500458, 0.0, 1446.0755615,
            2342.9626465, 6449.9116211, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow83"
        transform: [1365.5756836, -4.6532125, -3683.5307617, 0.0, -383.5206604, 66.1679077,
            -142.2641449, 0.0, 231.5857849, 1522.7667236, 83.9309235, 0.0, 76.5554276,
            2342.1166992, 8287.4228516, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow84"
        transform: [453.0384521, -4.653698, -3902.3007813, 0.0, -580.2296753, 94.4890366,
            -67.4745712, 0.0, 244.8840637, 1522.7667236, 26.6138897, 0.0, -556.7156372,
            2343.7565918, 10504.7998047, 1.0]
        coneColor: [0.0496681, 0.064744, 0.0581979, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow85"
        transform: [152.1455231, 6.5240693, 641.7928467, 0.0, 1591.7609863, -19.8864822,
            -377.1459351, 0.0, 12.8316154, 1343.8347168, -16.7024975, 0.0, 8197.3671875,
            2917.3439941, -4157.3217773, 1.0]
        coneColor: [0.0220645, 0.0259098, 0.0287785, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
    -   type: EveSpotlightSetItem
        name: "towerglow86"
        transform: [152.1455231, 6.5240693, 641.7928467, 0.0, 1591.7609863, -19.8864822,
            -377.1459351, 0.0, 12.8316154, 1343.8347168, -16.7024975, 0.0, 7845.2172852,
            2921.7436523, -4073.8847656, 1.0]
        coneColor: [0.0220645, 0.0259098, 0.0287785, 1.0]
        spriteColor: [0.7098039, 0.8352941, 0.8745098, 1.0]
        flareColor: [0.4470588, 0.7294118, 0.9215686, 1.0]
        spriteScale: [600.0, 1000.0, 250.0]
planeSets:
-   type: EvePlaneSet
    name: "DroneBay_Shield"
    planes:
    -   type: EvePlaneSetItem
        name: "Dronebay_Front_L2R"
        position: [6536.6279297, 161.7377319, 14372.5263672]
        scaling: [2627.1818848, 1096.2017822, 5.9863701]
        rotation: [0.9867961, 0.0, -0.1619672, 0.0]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [1.4, 0.005, 0.1, 1.0]
        layer2Transform: [2.4000001, 0.005, 0.0, 0.0]
        layer1Scroll: [0.02, 0.02, 0.5, 1.0]
        layer2Scroll: [-0.02, -0.02, 1.1, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Front_R2L"
        position: [6536.6279297, 161.7377319, 14372.5263672]
        scaling: [2624.1818848, 1098.4202881, 5.9863701]
        rotation: [0.9867961, 0.0, -0.1619672, 0.0]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [1.4, 0.005, 0.0, 0.0]
        layer2Transform: [1.8, 0.005, 0.0, 0.0]
        layer1Scroll: [0.02, 0.02, 0.5, 0.1]
        layer2Scroll: [0.02, 0.02, 0.1, 0.01]
    -   type: EvePlaneSetItem
        name: "Dronebay_Front_L2R"
        position: [6536.6279297, 161.7377319, 14372.5263672]
        scaling: [2627.1818848, 1096.2017822, 5.9863701]
        rotation: [0.1750061, 0.0, 0.9845674, 0.0]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [1.4, 0.005, 0.1, 1.0]
        layer2Transform: [2.4000001, 0.005, 0.0, 0.0]
        layer1Scroll: [0.02, 0.02, 0.5, 1.0]
        layer2Scroll: [-0.02, -0.02, 1.1, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Front_R2L"
        position: [6536.6279297, 161.7377319, 14372.5263672]
        scaling: [2624.1818848, 1098.4202881, 5.9863701]
        rotation: [0.1750061, 0.0, 0.9845674, 0.0]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [1.4, 0.005, 0.0, 0.0]
        layer2Transform: [1.8, 0.005, 0.0, 0.0]
        layer1Scroll: [0.02, 0.02, 0.5, 0.1]
        layer2Scroll: [0.02, 0.02, 0.1, 0.01]
    effect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/spaceobject/fx/planeglow.fx"
        parameters:
        -   type: Tr2Vector4Parameter
            name: "PlaneData"
            value: [0.0, 0.0, 0.0, 0.0]
        resources:
        -   type: TriTextureParameter
            name: "Layer2Map"
            resourcePath: "res:/texture/sprite/caustics.dds"
        -   type: TriTextureParameter
            name: "MaskMap"
            resourcePath: "res:/texture/sprite/softwhite.dds"
        -   type: TriTextureParameter
            name: "Layer1Map"
            resourcePath: "res:/texture/sprite/caustics.dds"
-   type: EvePlaneSet
    name: "DroneBay_Haze"
    planes:
    -   type: EvePlaneSetItem
        name: "Dronebay_Haze_base_01"
        position: [7741.0322266, 372.8520813, 17873.734375]
        scaling: [2856.5085449, 564.9655151, 14.6973896]
        rotation: [0.1668876, -0.0019381, 0.9859735, 0.0010171]
        color: [0.3686275, 0.4666667, 0.5137255, 1.0]
        layer1Transform: [10.0, 2.0, 1.0, 2.0]
        layer2Transform: [10.0, 2.0, 1.0, 2.0]
        layer1Scroll: [-0.05, 0.05, 1.0, 1.0]
        layer2Scroll: [0.05, -0.05, 0.0, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Haze_01"
        position: [7846.6938477, -44.6422844, 19717.6601563]
        scaling: [4628.4223633, 5677.2246094, 23.814291]
        rotation: [-0.051974, 0.5982724, 0.7959544, 0.0763254]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [2.0, 2.0, 1.0, 2.0]
        layer2Transform: [4.0, 3.0, 1.0, 2.0]
        layer1Scroll: [-0.06, 0.04, 1.0, 1.0]
        layer2Scroll: [0.06, -0.04, 0.0, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Haze_02"
        position: [8175.7192383, 708.3899536, 19717.6601563]
        scaling: [4628.4223633, 5671.8642578, 23.814291]
        rotation: [0.0986978, 0.8012527, 0.5847866, -0.0792302]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [2.0, 2.0, 1.0, 2.0]
        layer2Transform: [4.0, 3.0, 1.0, 2.0]
        layer1Scroll: [-0.05, 0.05, 1.0, 1.0]
        layer2Scroll: [0.05, -0.05, 0.0, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Haze_03"
        position: [8879.6435547, 376.5800781, 19989.1601563]
        scaling: [4628.4223633, 5671.8642578, 23.814291]
        rotation: [0.183092, 0.6800218, 0.6919123, -0.1590755]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [2.0, 2.0, 1.0, 2.0]
        layer2Transform: [4.0, 3.0, 1.0, 2.0]
        layer1Scroll: [-0.05, 0.05, 1.0, 1.0]
        layer2Scroll: [0.05, -0.05, 0.0, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Haze_02"
        position: [-4546.3466797, -4301.9042969, -6309.8984375]
        scaling: [4628.4223633, 5671.8642578, 23.814291]
        rotation: [0.9884447, -0.0120241, 0.1185817, 0.0936536]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [2.0, 2.0, 1.0, 2.0]
        layer2Transform: [4.0, 3.0, 1.0, 2.0]
        layer1Scroll: [-0.05, 0.05, 1.0, 1.0]
        layer2Scroll: [0.05, -0.05, 0.0, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Haze_02"
        position: [-3965.40625, -4301.9042969, -6810.5664063]
        scaling: [4628.4223633, 5671.8642578, 23.814291]
        rotation: [0.8789623, 0.0645375, 0.4722474, 0.0155756]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [2.0, 2.0, 1.0, 2.0]
        layer2Transform: [4.0, 3.0, 1.0, 2.0]
        layer1Scroll: [-0.05, 0.05, 1.0, 1.0]
        layer2Scroll: [0.05, -0.05, 0.0, 1.0]
    -   type: EvePlaneSetItem
        name: "Dronebay_Haze_02"
        position: [-4978.0361328, -4301.9042969, -6713.2939453]
        scaling: [4628.4223633, 5671.8642578, 23.814291]
        rotation: [-0.6783139, 0.0073538, 0.7341127, 0.0302452]
        color: [0.2392157, 0.3294118, 0.3764706, 1.0]
        layer1Transform: [2.0, 2.0, 1.0, 2.0]
        layer2Transform: [4.0, 3.0, 1.0, 2.0]
        layer1Scroll: [-0.05, 0.05, 1.0, 1.0]
        layer2Scroll: [0.05, -0.05, 0.0, 1.0]
    effect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/spaceobject/fx/planeglow.fx"
        parameters:
        -   type: Tr2Vector4Parameter
            name: "PlaneData"
            value: [1.0, 0.0, 0.0, 0.0]
        resources:
        -   type: TriTextureParameter
            name: "Layer2Map"
            resourcePath: "res:/texture/sprite/white2.dds"
        -   type: TriTextureParameter
            name: "MaskMap"
            resourcePath: "res:/texture/sprite/spotlight4.dds"
        -   type: TriTextureParameter
            name: "Layer1Map"
            resourcePath: "res:/texture/sprite/astroiddirt3.dds"
decals:
-   type: EveSpaceObjectDecal
    name: "Logo_GallenteLogo_Right"
    position: [13301.5263672, 1605.2752686, -11473.2978516]
    rotation: [-0.696192, 0.677861, 0.0456709, -0.231809]
    scaling: [29.7003288, 70.4995193, 55.485836]
    decalEffect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/decals/v3/decalv3.fx"
        parameters:
        -   type: Tr2Vector4Parameter
            name: "MaterialDiffuseColor"
            value: [0.5686275, 0.5686275, 0.5686275, 1.0]
        -   type: Tr2Vector4Parameter
            name: "MaterialReflectionColor"
        -   type: Tr2Vector4Parameter
            name: "MaterialSpecularCurve"
            value: [60.1795998, 160.0, 0.4203, 0.0]
        -   type: Tr2Vector4Parameter
            name: "MaterialSpecularFactors"
            value: [0.156, 1.0, 0.0, 0.0]
        -   type: Tr2Vector4Parameter
            name: "FresnelFactors"
            value: [2.0, 0.6, 0.0, 0.0]
        -   type: Tr2Vector4Parameter
            name: "ReflectionFactors"
            value: [0.156, 0.8, 0.75, 1.0]
        -   type: Tr2Vector4Parameter
            name: "ReflectionFactors2"
            value: [6.9000001, 0.0, 0.0, 0.0]
        resources:
        -   type: TriTextureParameter
            name: "NormalMap"
            resourcePath: "res:/dx9/model/station/gallente/gs2/gs2_n.dds"
        -   type: TriTextureParameter
            name: "DecalDiffuseMap"
            resourcePath: "res:/dx9/model/ship/gallente/decal/decal_gallentelogo02_d.dds"
        -   type: TriTextureParameter
            name: "DecalOsMap"
            resourcePath: "res:/dx9/model/ship/gallente/decal/decal_gallentelogo01_s.dds"
-   type: EveSpaceObjectDecal
    name: "L5_Numerical"
    position: [-8144.3334961, -650.6461182, 3418.8220215]
    rotation: [0.5143085, -0.498061, 0.4909571, 0.49637]
    scaling: [30.4832649, 19.8620834, 74.7787552]
    decalEffect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/decals/v3/decalv3.fx"
        parameters:
        -   type: Tr2Vector4Parameter
            name: "MaterialDiffuseColor"
            value: [0.5686275, 0.5686275, 0.5686275, 1.0]
        -   type: Tr2Vector4Parameter
            name: "MaterialReflectionColor"
        -   type: Tr2Vector4Parameter
            name: "MaterialSpecularCurve"
            value: [60.1795998, 160.0, 0.4203, 0.0]
        -   type: Tr2Vector4Parameter
            name: "MaterialSpecularFactors"
            value: [0.156, 1.0, 0.0, 0.0]
        -   type: Tr2Vector4Parameter
            name: "FresnelFactors"
            value: [2.0, 0.6, 0.0, 0.0]
        -   type: Tr2Vector4Parameter
            name: "ReflectionFactors"
            value: [0.156, 0.8, 0.75, 1.0]
        -   type: Tr2Vector4Parameter
            name: "ReflectionFactors2"
            value: [6.9000001, 0.0, 0.0, 0.0]
        resources:
        -   type: TriTextureParameter
            name: "NormalMap"
            resourcePath: "res:/dx9/model/station/gallente/gs1/gs1_n.dds"
        -   type: TriTextureParameter
            name: "DecalDiffuseMap"
            resourcePath: "res:/dx9/model/ship/gallente/decal/decal_l5y3n15_d.dds"
        -   type: TriTextureParameter
            name: "DecalOsMap"
            resourcePath: "res:/dx9/model/ship/gallente/decal/decal_shared02_s.dds"
-   type: EveSpaceObjectDecal
    name: "NumberPlate"
    position: [6059.9746094, 1819.7196045, 17670.4726563]
    rotation: [0.5102304, 0.5290959, -0.4830666, 0.4757827]
    scaling: [110.9141159, 129.4034729, 76.4764938]
    decalEffect:
        type: Tr2Effect
        effectFilePath: "res:/graphics/effect/managed/space/decals/v3/decalv3.fx"
        parameters:
        -   type: Tr2Vector4Parameter
            name: "MaterialDiffuseColor"
            value: [0.5686275, 0.5686275, 0.5686275, 1.0]
        -   type: Tr2Vector4Parameter
            name: "MaterialReflectionColor"
        -   type: Tr2Vector4Parameter
            name: "MaterialSpecularCurve"
            value: [60.1795998, 160.0, 0.4203, 0.0]
        -   type: Tr2Vector4Parameter
            name: "MaterialSpecularFactors"
            value: [0.156, 1.0, 0.0, 0.0]
        -   type: Tr2Vector4Parameter
            name: "FresnelFactors"
            value: [2.0, 0.6, 0.0, 0.0]
        -   type: Tr2Vector4Parameter
            name: "ReflectionFactors"
            value: [0.156, 0.8, 0.75, 1.0]
        -   type: Tr2Vector4Parameter
            name: "ReflectionFactors2"
            value: [6.9000001, 0.0, 0.0, 0.0]
        resources:
        -   type: TriTextureParameter
            name: "NormalMap"
            resourcePath: "res:/dx9/model/station/gallente/gs2/gs2_n.dds"
        -   type: TriTextureParameter
            name: "DecalDiffuseMap"
            resourcePath: "res:/dx9/model/ship/gallente/decal/decal_numberplateleft_d.dds"
        -   type: TriTextureParameter
            name: "DecalOsMap"
            resourcePath: "res:/dx9/model/ship/gallente/decal/decal_shared_s.dds"
boundingSphereCenter: [3605.8198242, -3378.9013672, -707.1208496]
boundingSphereRadius: 22018.4316406
"""
        self.assertChecksumsEqual(longString)


    def testAnotherLongString(self):
        longString = b"""
type: Tr2HighLevelShader
name: "BoxLight"
shaderPath: "res:/graphics/shaders/BoxLight.fx"
permuteTags:
-   type: Tr2ShaderPermuteTag
    name: "Shadow"
    permuteDefineString: "OPT_SHADOW"
    tagBits: 1
-   type: Tr2ShaderPermuteTag
    name: "ProjectedTexture"
    permuteDefineString: "OPT_PROJECTED_TEXTURE"
    tagBits: 2
-   type: Tr2ShaderPermuteTag
    name: "GenerateSHCoefficients"
    permuteDefineString: "OPT_GENERATE_SH"
    tagBits: 4
    unused: "OPT_GENERATE_LIGHTMAP"
-   type: Tr2ShaderPermuteTag
    name: "OPT_INTERIOR_SM_HIGH"
    permuteDefineString: "OPT_INTERIOR_SM_HIGH"
    tagBits: 8
-   type: Tr2ShaderPermuteTag
    name: "OPT_INTZ"
    permuteDefineString: "OPT_INTZ"
    tagBits: 16
    unused: "OPT_GENERATE_SH or OPT_GENERATE_LIGHTMAP"
-   type: Tr2ShaderPermuteTag
    name: "GenerateLightMap"
    permuteDefineString: "OPT_GENERATE_LIGHTMAP"
    tagBits: 32
-   type: Tr2ShaderPermuteTag
    name: "AdditionalBoundingBox"
    permuteDefineString: "OPT_BOUNDING_BOX"
    tagBits: 64
renderClass: "lightpass"
        """
        self.assertChecksumsEqual(longString)
