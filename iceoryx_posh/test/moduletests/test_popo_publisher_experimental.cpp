// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "iceoryx_posh/capro/service_description.hpp"
#include "iceoryx_posh/experimental/popo/publisher.hpp"
#include "iceoryx_posh/capro/service_description.hpp"
#include "iceoryx_posh/mepoo/chunk_header.hpp"
#include "iceoryx_utils/cxx/expected.hpp"
#include "iceoryx_utils/cxx/optional.hpp"
#include "iceoryx_utils/cxx/unique_ptr.hpp"

#include "test.hpp"

#include <iostream>

using namespace ::testing;
using ::testing::_;

// ========================= Mocks ========================= //

class MockPublisherPortUser
{
public:
    MockPublisherPortUser(std::nullptr_t)
    {}
    MOCK_METHOD1(allocateChunk, iox::cxx::expected<iox::mepoo::ChunkHeader*, iox::popo::AllocationError>(const uint32_t));
    MOCK_METHOD1(freeChunk, void(iox::mepoo::ChunkHeader* const));
    MOCK_METHOD1(sendChunk, void(iox::mepoo::ChunkHeader* const));
    MOCK_METHOD0(getLastChunk, iox::cxx::optional<iox::mepoo::ChunkHeader*>());
    MOCK_METHOD0(offer, void());
    MOCK_METHOD0(stopOffer, void());
    MOCK_METHOD0(isOffered, bool());
    MOCK_METHOD0(hasSubscribers, bool());
};

template<typename T>
class MockBasePublisher : public iox::popo::PublisherInterface<T>
{
public:
    void publish(iox::popo::Sample<T>& sample) noexcept
    {
        return publishMocked(sample);
    };

    MockBasePublisher(const iox::capro::ServiceDescription& service){};
    MOCK_CONST_METHOD0(uid, iox::popo::uid_t());
    MOCK_METHOD1_T(loan, iox::cxx::expected<iox::popo::Sample<T>, iox::popo::AllocationError>(uint32_t));
    MOCK_METHOD1_T(publishMocked, void(iox::popo::Sample<T>&) noexcept);
    MOCK_METHOD0_T(previousSample, iox::cxx::optional<iox::popo::Sample<T>>());
    MOCK_METHOD0(offer, void(void));
    MOCK_METHOD0(stopOffer, void(void));
    MOCK_METHOD0(isOffered, bool(void));
    MOCK_METHOD0(hasSubscribers, bool(void));
};

struct DummyData{
    uint64_t val = 42;
};

// ========================= Tested Classes ========================= //

template<typename T, typename port_t>
class StubbedBasePublisher : public iox::popo::BasePublisher<T, port_t>
{
public:
    StubbedBasePublisher(iox::capro::ServiceDescription sd)
        : iox::popo::BasePublisher<T, port_t>::BasePublisher(sd)
    {};
    uid_t uid() const noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::uid();
    }
    iox::cxx::expected<iox::popo::Sample<T>, iox::popo::AllocationError> loan(uint64_t size) noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::loan(size);
    }
    void release(iox::popo::Sample<T>& sample) noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::release(sample);
    }
    void publish(iox::popo::Sample<T>& sample) noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::publish(sample);
    }
    iox::cxx::optional<iox::popo::Sample<T>> previousSample() noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::previousSample();
    }
    void offer() noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::offer();
    }
    void stopOffer() noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::stopOffer();
    }
    bool isOffered() noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::isOffered();
    }
    bool hasSubscribers() noexcept
    {
        return iox::popo::BasePublisher<T, port_t>::hasSubscribers();
    }
    port_t& getMockedPort()
    {
        return iox::popo::BasePublisher<T, port_t>::m_port;
    }
};


using TestBasePublisher = StubbedBasePublisher<DummyData, MockPublisherPortUser>;
using TestTypedPublisher = iox::popo::TypedPublisher<DummyData, MockBasePublisher<DummyData>>;
using TestUntypedPublisher = iox::popo::UntypedPublisher;

// ========================= Base Publisher Tests ========================= //

class ExperimentalBasePublisherTest : public Test {

public:
    ExperimentalBasePublisherTest()
    {

    }

    void SetUp()
    {
    }

    void TearDown()
    {
    }

protected:
    TestBasePublisher sut{{"", "", ""}};
};

TEST_F(ExperimentalBasePublisherTest, LoanForwardsAllocationErrorsToCaller)
{
    // ===== Setup ===== //
    ON_CALL(sut.getMockedPort(), allocateChunk).WillByDefault(Return(ByMove(iox::cxx::error<iox::popo::AllocationError>(iox::popo::AllocationError::RUNNING_OUT_OF_CHUNKS))));
    // ===== Test ===== //
    auto result = sut.loan(sizeof(DummyData));
    // ===== Verify ===== //
    EXPECT_EQ(true, result.has_error());
    EXPECT_EQ(iox::popo::AllocationError::RUNNING_OUT_OF_CHUNKS, result.get_error());
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, LoanReturnsAllocatedSampleOnSuccess)
{
    // ===== Setup ===== //
    auto chunkHeader = new iox::mepoo::ChunkHeader();
    ON_CALL(sut.getMockedPort(), allocateChunk)
            .WillByDefault(Return(ByMove(iox::cxx::success<iox::mepoo::ChunkHeader*>(chunkHeader))));
    // ===== Test ===== //
    auto result = sut.loan(sizeof(DummyData));
    // ===== Verify ===== //
    // The memory location of the sample should be the same as the chunk payload.
    EXPECT_EQ(chunkHeader->payload(), result.get_value().get());
    // ===== Cleanup ===== //
    delete chunkHeader;
}

TEST_F(ExperimentalBasePublisherTest, LoanedSamplesAreAutomaticallyReleasedWhenOutOfScope)
{
    // ===== Setup ===== //
    auto chunkHeader = new iox::mepoo::ChunkHeader();

    ON_CALL(sut.getMockedPort(), allocateChunk)
            .WillByDefault(Return(ByMove(iox::cxx::success<iox::mepoo::ChunkHeader*>(chunkHeader))));
    EXPECT_CALL(sut.getMockedPort(), freeChunk(chunkHeader)).Times(1);
    // ===== Test ===== //
    {
        auto result = sut.loan(sizeof(DummyData));
    }
    // ===== Verify ===== //
    // ===== Cleanup ===== //
    delete chunkHeader;
}

TEST_F(ExperimentalBasePublisherTest, OffersServiceWhenTryingToPublishOnUnofferedService)
{
    // ===== Setup ===== //
    ON_CALL(sut.getMockedPort(), allocateChunk).WillByDefault(Return(ByMove(iox::cxx::success<iox::mepoo::ChunkHeader*>())));
    EXPECT_CALL(sut.getMockedPort(), offer).Times(1);
    // ===== Test ===== //
    sut.loan(sizeof(DummyData)).and_then([](iox::popo::Sample<DummyData>& sample){
        sample.publish();
    });
    // ===== Verify ===== //
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, PublishingSendsUnderlyingMemoryChunkOnPublisherPort)
{
    // ===== Setup ===== //
    ON_CALL(sut.getMockedPort(), allocateChunk).WillByDefault(Return(ByMove(iox::cxx::success<iox::mepoo::ChunkHeader*>())));
    EXPECT_CALL(sut.getMockedPort(), sendChunk).Times(1);
    // ===== Test ===== //
    sut.loan(sizeof(DummyData)).and_then([](iox::popo::Sample<DummyData>& sample){
        sample.publish();
    });
    // ===== Verify ===== //
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, PreviousSampleReturnsSampleWhenPreviousChunkIsRetrievable)
{
    // ===== Setup ===== //
    EXPECT_CALL(sut.getMockedPort(), getLastChunk).WillOnce(Return(ByMove(iox::cxx::make_optional<iox::mepoo::ChunkHeader*>())));
    // ===== Test ===== //
    auto result = sut.previousSample();
    // ===== Verify ===== //
    EXPECT_EQ(true, result.has_value());
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, PreviousSampleReturnsEmptyOptionalWhenChunkNotRetrievable)
{
    // ===== Setup ===== //
    EXPECT_CALL(sut.getMockedPort(), getLastChunk).WillOnce(Return(ByMove(iox::cxx::nullopt)));
    // ===== Test ===== //
    auto result = sut.previousSample();
    // ===== Verify ===== //
    EXPECT_EQ(false, result.has_value());
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, OfferDoesOfferServiceOnUnderlyingPort)
{
    // ===== Setup ===== //
    EXPECT_CALL(sut.getMockedPort(), offer).Times(1);
    // ===== Test ===== //
    sut.offer();
    // ===== Verify ===== //
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, StopOfferDoesStopOfferServiceOnUnderlyingPort)
{
    // ===== Setup ===== //
    EXPECT_CALL(sut.getMockedPort(), stopOffer).Times(1);
    // ===== Test ===== //
    sut.stopOffer();
    // ===== Verify ===== //
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, isOfferedDoesCheckIfPortIsOfferedOnUnderlyingPort)
{
    // ===== Setup ===== //
    EXPECT_CALL(sut.getMockedPort(), isOffered).Times(1);
    // ===== Test ===== //
    sut.isOffered();
    // ===== Verify ===== //
    // ===== Cleanup ===== //
}

TEST_F(ExperimentalBasePublisherTest, isOfferedDoesCheckIfUnderylingPortHasSubscribers)
{
    // ===== Setup ===== //
    EXPECT_CALL(sut.getMockedPort(), hasSubscribers).Times(1);
    // ===== Test ===== //
    sut.hasSubscribers();
    // ===== Verify ===== //
    // ===== Cleanup ===== //
}

// ========================= Typed Publisher Tests ========================= //

class ExperimentalTypedPublisherTest : public Test {

public:
    ExperimentalTypedPublisherTest()
    {

    }

    void SetUp()
    {
    }

    void TearDown()
    {
    }

protected:
    TestTypedPublisher sut{{"", "", ""}};
};

TEST_F(ExperimentalTypedPublisherTest, CanLoanSamplesAndPublisheTheResultOfALambdaWithNoAdditionalArguments)
{
    // ===== Setup ===== //
    auto chunk = new iox::mepoo::ChunkHeader();
    auto sample = new iox::popo::Sample<DummyData>(iox::cxx::unique_ptr<DummyData>(
                                                        reinterpret_cast<DummyData*>(reinterpret_cast<DummyData*>(chunk->payload())),
                                                        [](DummyData* const){} // Placeholder deleter.
                                                    ),
                                                    sut);
    EXPECT_CALL(sut, loan).WillOnce(Return(ByMove(iox::cxx::success<iox::popo::Sample<DummyData>>(std::move(*sample)))));
    EXPECT_CALL(sut, publishMocked).Times(1);
    // ===== Test ===== //
    auto result = sut.publishResultOf([](DummyData* allocation){
        auto data = new (allocation) DummyData();
        data->val = 777;
    });
    // ===== Verify ===== //
    EXPECT_EQ(false, result.has_error());
    // ===== Cleanup ===== //
    delete sample;
    delete chunk;
}

TEST_F(ExperimentalTypedPublisherTest, CanLoanSamplesAndPublishTheResultOfACallableStructWithNoAdditionalArguments)
{
    // ===== Setup ===== //
    struct CallableStruct{
        void operator()(DummyData* allocation){
            auto data = new (allocation) DummyData();
            data->val = 777;
        };
    };
    auto chunk = new iox::mepoo::ChunkHeader();
    auto sample = new iox::popo::Sample<DummyData>(iox::cxx::unique_ptr<DummyData>(
                                                        reinterpret_cast<DummyData*>(reinterpret_cast<DummyData*>(chunk->payload())),
                                                        [](DummyData* const){} // Placeholder deleter.
                                                    ),
                                                    sut);
    EXPECT_CALL(sut, loan).WillOnce(Return(ByMove(iox::cxx::success<iox::popo::Sample<DummyData>>(std::move(*sample)))));
    EXPECT_CALL(sut, publishMocked).Times(1);
    // ===== Test ===== //
    auto result = sut.publishResultOf(CallableStruct{});
    // ===== Verify ===== //
    EXPECT_EQ(false, result.has_error());
    // ===== Cleanup ===== //
    delete sample;
    delete chunk;
}

TEST_F(ExperimentalTypedPublisherTest, CanLoanSamplesAndPublishTheResultOfACallableStructWithAdditionalArguments)
{
    // ===== Setup ===== //
    struct CallableStruct{
        void operator()(DummyData* allocation, int intVal, float floatVal){
            auto data = new (allocation) DummyData();
            data->val = 777;
        };
    };
    auto chunk = new iox::mepoo::ChunkHeader();
    auto sample = new iox::popo::Sample<DummyData>(iox::cxx::unique_ptr<DummyData>(
                                                        reinterpret_cast<DummyData*>(reinterpret_cast<DummyData*>(chunk->payload())),
                                                        [](DummyData* const){} // Placeholder deleter.
                                                    ),
                                                    sut);
    EXPECT_CALL(sut, loan).WillOnce(Return(ByMove(iox::cxx::success<iox::popo::Sample<DummyData>>(std::move(*sample)))));
    EXPECT_CALL(sut, publishMocked).Times(1);
    // ===== Test ===== //
    auto result = sut.publishResultOf(CallableStruct{}, 42, 77.77);
    // ===== Verify ===== //
    EXPECT_EQ(false, result.has_error());
    // ===== Cleanup ===== //
    delete sample;
    delete chunk;
}

void freeFunctionNoAdditionalArgs(DummyData* allocation)
{
    auto data = new (allocation) DummyData();
    data->val = 777;
}
void freeFunctionWithAdditionalArgs(DummyData* allocation, int intVal, float floatVal)
{
    auto data = new (allocation) DummyData();
    data->val = 777;
}

TEST_F(ExperimentalTypedPublisherTest, CanLoanSamplesAndPublishTheResultOfFunctionPointerWithNoAdditionalArguments)
{
    // ===== Setup ===== //
    auto chunk = new iox::mepoo::ChunkHeader();
    auto sample = new iox::popo::Sample<DummyData>(iox::cxx::unique_ptr<DummyData>(
                                                        reinterpret_cast<DummyData*>(reinterpret_cast<DummyData*>(chunk->payload())),
                                                        [](DummyData* const){} // Placeholder deleter.
                                                    ),
                                                    sut);
    EXPECT_CALL(sut, loan).WillOnce(Return(ByMove(iox::cxx::success<iox::popo::Sample<DummyData>>(std::move(*sample)))));
    EXPECT_CALL(sut, publishMocked).Times(1);
    // ===== Test ===== //
    auto result = sut.publishResultOf(freeFunctionNoAdditionalArgs);
    // ===== Verify ===== //
    EXPECT_EQ(false, result.has_error());
    // ===== Cleanup ===== //
    delete sample;
    delete chunk;
}

TEST_F(ExperimentalTypedPublisherTest, CanLoanSamplesAndPublishTheResultOfFunctionPointerWithAdditionalArguments)
{
    // ===== Setup ===== //
    auto chunk = new iox::mepoo::ChunkHeader();
    auto sample = new iox::popo::Sample<DummyData>(iox::cxx::unique_ptr<DummyData>(
                                                        reinterpret_cast<DummyData*>(reinterpret_cast<DummyData*>(chunk->payload())),
                                                        [](DummyData* const){} // Placeholder deleter.
                                                    ),
                                                    sut);
    EXPECT_CALL(sut, loan).WillOnce(Return(ByMove(iox::cxx::success<iox::popo::Sample<DummyData>>(std::move(*sample)))));
    EXPECT_CALL(sut, publishMocked).Times(1);
    // ===== Test ===== //
    auto result = sut.publishResultOf(freeFunctionWithAdditionalArgs, 42, 77.77);
    // ===== Verify ===== //
    EXPECT_EQ(false, result.has_error());
    // ===== Cleanup ===== //
    delete sample;
    delete chunk;
}

TEST_F(ExperimentalTypedPublisherTest, CanLoanSamplesAndPublishCopiesOfProvidedValues)
{
    // ===== Setup ===== //
    auto chunk = new iox::mepoo::ChunkHeader();
    auto sample = new iox::popo::Sample<DummyData>(iox::cxx::unique_ptr<DummyData>(
                                                        reinterpret_cast<DummyData*>(reinterpret_cast<DummyData*>(chunk->payload())),
                                                        [](DummyData* const){} // Placeholder deleter.
                                                    ),
                                                    sut);
    auto data = DummyData();
    data.val = 777;
    EXPECT_CALL(sut, loan).WillOnce(Return(ByMove(iox::cxx::success<iox::popo::Sample<DummyData>>(std::move(*sample)))));
    EXPECT_CALL(sut, publishMocked).Times(1);
    // ===== Test ===== //
    auto result = sut.publishCopyOf(data);
    // ===== Verify ===== //
    EXPECT_EQ(false, result.has_error());
    // ===== Cleanup ===== //
    delete sample;
    delete chunk;
}

// ========================= Untyped Publisher Tests ========================= //


