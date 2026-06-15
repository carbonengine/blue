// Copyright © 2023 CCP ehf.

const char* g_moduleName = "blue"; // required by inlined new / delete operators

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
