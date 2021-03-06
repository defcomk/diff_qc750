/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "RTSPSource"
#include <utils/Log.h>

#include "RTSPSource.h"

#include "AnotherPacketSource.h"
#include "MyHandler.h"

#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MetaData.h>

namespace android {

NuPlayer::RTSPSource::RTSPSource(
        const char *url,
        const KeyedVector<String8, String8> *headers,
        bool uidValid,
        uid_t uid)
    : mURL(url),
      mUIDValid(uidValid),
      mUID(uid),
      mFlags(0),
      mState(DISCONNECTED),
      mFinalResult(OK),
      mDisconnectReplyID(0),
#ifdef PROFILING
      mVideoTrackIndex(0),
      mDamagedVideoAccessUnits(0),
      mDamagedAudioAccessUnits(0),
#endif
      mStartingUp(true),
      mSeekGeneration(0) {
    if (headers) {
        mExtraHeaders = *headers;

        ssize_t index =
            mExtraHeaders.indexOfKey(String8("x-hide-urls-from-log"));

        if (index >= 0) {
            mFlags |= kFlagIncognito;

            mExtraHeaders.removeItemsAt(index);
        }
    }
}

NuPlayer::RTSPSource::~RTSPSource() {
    if (mLooper != NULL) {
        mLooper->stop();
    }
}

void NuPlayer::RTSPSource::start() {
    if (mLooper == NULL) {
        mLooper = new ALooper;
        mLooper->setName("rtsp");
        mLooper->start();

        mReflector = new AHandlerReflector<RTSPSource>(this);
        mLooper->registerHandler(mReflector);
    }

    CHECK(mHandler == NULL);

    sp<AMessage> notify = new AMessage(kWhatNotify, mReflector->id());

    mHandler = new MyHandler(mURL.c_str(), notify, mUIDValid, mUID);
    mLooper->registerHandler(mHandler);

    CHECK_EQ(mState, (int)DISCONNECTED);
    mState = CONNECTING;

    mHandler->connect();
}

void NuPlayer::RTSPSource::stop() {
    sp<AMessage> msg = new AMessage(kWhatDisconnect, mReflector->id());

    sp<AMessage> dummy;
    msg->postAndAwaitResponse(&dummy);
}

status_t NuPlayer::RTSPSource::feedMoreTSData() {
    return mFinalResult;
}

sp<MetaData> NuPlayer::RTSPSource::getFormatMeta(bool audio) {
    sp<AnotherPacketSource> source = getSource(audio);

    if (source == NULL) {
        return NULL;
    }

    return source->getFormat();
}

bool NuPlayer::RTSPSource::haveSufficientDataOnAllTracks() {
    // We're going to buffer at least 2 secs worth data on all tracks before
    // starting playback (both at startup and after a seek).

    static const int64_t kMinDurationUs = 2000000ll;

    status_t err;
    int64_t durationUs;
    if (mAudioTrack != NULL
            && (durationUs = mAudioTrack->getBufferedDurationUs(&err))
                    < kMinDurationUs
            && err == OK) {
        ALOGV("audio track doesn't have enough data yet. (%.2f secs buffered)",
              durationUs / 1E6);
        return false;
    }

    if (mVideoTrack != NULL
            && (durationUs = mVideoTrack->getBufferedDurationUs(&err))
                    < kMinDurationUs
            && err == OK) {
        ALOGV("video track doesn't have enough data yet. (%.2f secs buffered)",
              durationUs / 1E6);
        return false;
    }

    return true;
}

status_t NuPlayer::RTSPSource::dequeueAccessUnit(
        bool audio, sp<ABuffer> *accessUnit) {
    if (mStartingUp) {
        if (!haveSufficientDataOnAllTracks()) {
            return -EWOULDBLOCK;
        }

        mStartingUp = false;
    }

    sp<AnotherPacketSource> source = getSource(audio);

    if (source == NULL) {
        return -EWOULDBLOCK;
    }

    status_t finalResult;
    if (!source->hasBufferAvailable(&finalResult)) {
        return finalResult == OK ? -EWOULDBLOCK : finalResult;
    }

    return source->dequeueAccessUnit(accessUnit);
}

status_t NuPlayer::RTSPSource::queueCsd(
        bool audio, sp<ABuffer> &csd) {
    sp<AnotherPacketSource> source = getSource(audio);

    if (source == NULL) {
        return -EWOULDBLOCK;
    }

    source->queueAccessUnit(csd);
    return OK;
}

void NuPlayer::RTSPSource::pause() {
     if (mState == CONNECTED && mHandler != NULL) {
         mHandler->pause();
     }
}

void NuPlayer::RTSPSource::resume() {
     if (mState == CONNECTED && mHandler != NULL) {
         mHandler->resume();
     }
}

sp<AnotherPacketSource> NuPlayer::RTSPSource::getSource(bool audio) {
    if (mTSParser != NULL) {
        sp<MediaSource> source = mTSParser->getSource(
                audio ? ATSParser::AUDIO : ATSParser::VIDEO);

        return static_cast<AnotherPacketSource *>(source.get());
    }

    return audio ? mAudioTrack : mVideoTrack;
}

status_t NuPlayer::RTSPSource::getDuration(int64_t *durationUs) {
    *durationUs = 0ll;

    int64_t audioDurationUs;
    if (mAudioTrack != NULL
            && mAudioTrack->getFormat()->findInt64(
                kKeyDuration, &audioDurationUs)
            && audioDurationUs > *durationUs) {
        *durationUs = audioDurationUs;
    }

    int64_t videoDurationUs;
    if (mVideoTrack != NULL
            && mVideoTrack->getFormat()->findInt64(
                kKeyDuration, &videoDurationUs)
            && videoDurationUs > *durationUs) {
        *durationUs = videoDurationUs;
    }

    return OK;
}

#ifdef PROFILING
void NuPlayer::RTSPSource::getDamagedAccessUnits(int32_t *damagedAccessUnits, bool audio) {
    if (audio) {
        *damagedAccessUnits = mDamagedAudioAccessUnits;
    } else {
        *damagedAccessUnits = mDamagedVideoAccessUnits;
    }
}
#endif

status_t NuPlayer::RTSPSource::seekTo(int64_t seekTimeUs) {
    sp<AMessage> msg = new AMessage(kWhatPerformSeek, mReflector->id());
    msg->setInt32("generation", ++mSeekGeneration);
    msg->setInt64("timeUs", seekTimeUs);
    msg->post(200000ll);

    return OK;
}

void NuPlayer::RTSPSource::performSeek(int64_t seekTimeUs) {
    if (mState != CONNECTED && mHandler != NULL) {
        mHandler->setPlayStartRange(seekTimeUs);
        return;
    }

    if (mHandler != NULL) {
        mState = SEEKING;
        mHandler->seek(seekTimeUs);
    }
}

bool NuPlayer::RTSPSource::isSeekable() {
    return true;
}

uint32_t NuPlayer::RTSPSource::flags() const {
    return FLAG_SEEKABLE;
}

void NuPlayer::RTSPSource::onMessageReceived(const sp<AMessage> &msg) {
    if (msg->what() == kWhatDisconnect) {
        uint32_t replyID;
        CHECK(msg->senderAwaitsResponse(&replyID));

        mDisconnectReplyID = replyID;
        finishDisconnectIfPossible();
        return;
    } else if (msg->what() == kWhatPerformSeek) {
        int32_t generation;
        CHECK(msg->findInt32("generation", &generation));

        if (generation != mSeekGeneration) {
            // obsolete.
            return;
        }

        int64_t seekTimeUs;
        CHECK(msg->findInt64("timeUs", &seekTimeUs));

        performSeek(seekTimeUs);
        return;
    }

    CHECK_EQ(msg->what(), (int)kWhatNotify);

    int32_t what;
    CHECK(msg->findInt32("what", &what));

    switch (what) {
        case MyHandler::kWhatConnected:
            onConnected();
            break;

        case MyHandler::kWhatDisconnected:
            onDisconnected(msg);
            break;

        case MyHandler::kWhatSeekDone:
        {
            mState = CONNECTED;
            mStartingUp = true;
            break;
        }

        case MyHandler::kWhatAccessUnit:
        {
            size_t trackIndex;
            CHECK(msg->findSize("trackIndex", &trackIndex));

            if (mTSParser == NULL) {
                CHECK_LT(trackIndex, mTracks.size());
            } else {
                CHECK_EQ(trackIndex, 0u);
            }

            sp<ABuffer> accessUnit;
            CHECK(msg->findBuffer("accessUnit", &accessUnit));

            int32_t damaged;
            if (accessUnit->meta()->findInt32("damaged", &damaged)
                    && damaged) {
                ALOGI("dropping damaged access unit.");
#ifdef PROFILING
                    if (trackIndex == mVideoTrackIndex && mVideoTrack != NULL) {
                        ++mDamagedVideoAccessUnits;
                    } else {
                        ++mDamagedAudioAccessUnits;
                    }
#endif
                break;
            }

            if (mTSParser != NULL) {
                size_t offset = 0;
                status_t err = OK;
                while (offset + 188 <= accessUnit->size()) {
                    err = mTSParser->feedTSPacket(
                            accessUnit->data() + offset, 188);
                    if (err != OK) {
                        break;
                    }

                    offset += 188;
                }

                if (offset < accessUnit->size()) {
                    err = ERROR_MALFORMED;
                }

                if (err != OK) {
                    sp<AnotherPacketSource> source = getSource(false /* audio */);
                    if (source != NULL) {
                        source->signalEOS(err);
                    }

                    source = getSource(true /* audio */);
                    if (source != NULL) {
                        source->signalEOS(err);
                    }
                }
                break;
            }

            TrackInfo *info = &mTracks.editItemAt(trackIndex);

            sp<AnotherPacketSource> source = info->mSource;
            if (source != NULL) {
                uint32_t rtpTime;
                CHECK(accessUnit->meta()->findInt32("rtp-time", (int32_t *)&rtpTime));

                if (!info->mNPTMappingValid) {
                    // This is a live stream, we didn't receive any normal
                    // playtime mapping. Assume the first packets correspond
                    // to time 0.

                    ALOGV("This is a live stream, assuming time = 0");

                    info->mRTPTime = rtpTime;
                    info->mNormalPlaytimeUs = 0ll;
                    info->mNPTMappingValid = true;
                }

                int64_t nptUs =
                    ((double)rtpTime - (double)info->mRTPTime)
                        / info->mTimeScale
                        * 1000000ll
                        + info->mNormalPlaytimeUs;

                accessUnit->meta()->setInt64("timeUs", nptUs);

                source->queueAccessUnit(accessUnit);
            }
            break;
        }

        case MyHandler::kWhatEOS:
        {
            int32_t finalResult;
            CHECK(msg->findInt32("finalResult", &finalResult));
            CHECK_NE(finalResult, (status_t)OK);

            if (mTSParser != NULL) {
                sp<AnotherPacketSource> source = getSource(false /* audio */);
                if (source != NULL) {
                    source->signalEOS(finalResult);
                }

                source = getSource(true /* audio */);
                if (source != NULL) {
                    source->signalEOS(finalResult);
                }

                return;
            }

            size_t trackIndex;
            CHECK(msg->findSize("trackIndex", &trackIndex));
            CHECK_LT(trackIndex, mTracks.size());

            TrackInfo *info = &mTracks.editItemAt(trackIndex);
            sp<AnotherPacketSource> source = info->mSource;
            if (source != NULL) {
                source->signalEOS(finalResult);
            }

            break;
        }

        case MyHandler::kWhatSeekDiscontinuity:
        {
            if ((mState == CONNECTED) || (mState == SEEKING)) {
                size_t trackIndex;
                CHECK(msg->findSize("trackIndex", &trackIndex));
                CHECK_LT(trackIndex, mTracks.size());

                TrackInfo *info = &mTracks.editItemAt(trackIndex);
                sp<AnotherPacketSource> source = info->mSource;
                if (source != NULL) {
                    source->queueDiscontinuity(ATSParser::DISCONTINUITY_SEEK, NULL);
                }
            }

            break;
        }

        case MyHandler::kWhatNormalPlayTimeMapping:
        {
            size_t trackIndex;
            CHECK(msg->findSize("trackIndex", &trackIndex));
            CHECK_LT(trackIndex, mTracks.size());

            uint32_t rtpTime;
            CHECK(msg->findInt32("rtpTime", (int32_t *)&rtpTime));

            int64_t nptUs;
            CHECK(msg->findInt64("nptUs", &nptUs));

            TrackInfo *info = &mTracks.editItemAt(trackIndex);
            info->mRTPTime = rtpTime;
            info->mNormalPlaytimeUs = nptUs;
            info->mNPTMappingValid = true;
            break;
        }

        default:
            TRESPASS();
    }
}

void NuPlayer::RTSPSource::onConnected() {
    CHECK(mAudioTrack == NULL);
    CHECK(mVideoTrack == NULL);
    CHECK(mHandler != NULL);

    size_t numTracks = mHandler->countTracks();
    for (size_t i = 0; i < numTracks; ++i) {
        int32_t timeScale;
        sp<MetaData> format = mHandler->getTrackFormat(i, &timeScale);

        const char *mime;
        CHECK(format->findCString(kKeyMIMEType, &mime));

        if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG2TS)) {
            // Very special case for MPEG2 Transport Streams.
            CHECK_EQ(numTracks, 1u);

            mTSParser = new ATSParser;
            return;
        }

        bool isAudio = !strncasecmp(mime, "audio/", 6);
        bool isVideo = !strncasecmp(mime, "video/", 6);

        TrackInfo info;
        info.mTimeScale = timeScale;
        info.mRTPTime = 0;
        info.mNormalPlaytimeUs = 0ll;
        info.mNPTMappingValid = false;

        if ((isAudio && mAudioTrack == NULL)
                || (isVideo && mVideoTrack == NULL)) {
            sp<AnotherPacketSource> source = new AnotherPacketSource(format);

            if (isAudio) {
                mAudioTrack = source;
            } else {
#ifdef PROFILING
                mVideoTrackIndex = i;
#endif
                mVideoTrack = source;
            }

            info.mSource = source;
        }

        mTracks.push(info);
    }

    mState = CONNECTED;
}

void NuPlayer::RTSPSource::onDisconnected(const sp<AMessage> &msg) {
    status_t err;
    CHECK(msg->findInt32("result", &err));
    CHECK_NE(err, (status_t)OK);

    mLooper->unregisterHandler(mHandler->id());
    mHandler.clear();

    mState = DISCONNECTED;
    mFinalResult = err;

    if (mDisconnectReplyID != 0) {
        finishDisconnectIfPossible();
    }
}

void NuPlayer::RTSPSource::finishDisconnectIfPossible() {
    if (mState != DISCONNECTED && mHandler != NULL) {
        mHandler->disconnect();
        return;
    }

    (new AMessage)->postReply(mDisconnectReplyID);
    mDisconnectReplyID = 0;
}

}  // namespace android
