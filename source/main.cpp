#include <gtest\gtest.h>

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    //::testing::GTEST_FLAG(filter) = "sampleParser.parseEcgPacketEncrypt";
    //::testing::GTEST_FLAG(filter) = "nodeparser/TokensSizeTest.getTokenSize/1";
    //::testing::GTEST_FLAG(filter) = "SampleParser/ProperSamplesTest.parseEcgPacketByParts/12";
    //::testing::GTEST_FLAG(filter) = "module.list";
    //::testing::GTEST_FLAG(filter) = "SampleParser*";
    //::testing::GTEST_FLAG(filter) = "continuationTest.cancelableTaskNotScheduled";
    //::testing::GTEST_FLAG(filter) = "continuationTest.basicAssumptions2";
    //::testing::GTEST_FLAG(filter) = "parser/TestParserComplex.properParsing/*";

    return RUN_ALL_TESTS();
}