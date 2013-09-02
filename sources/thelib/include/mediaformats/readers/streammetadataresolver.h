/*
 *  Copyright (c) 2010,
 *  Gavriloaie Eugen-Andrei (shiretu@gmail.com)
 *
 *  This file is part of crtmpserver.
 *  crtmpserver is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  crtmpserver is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with crtmpserver.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _STREAMMETADATARESOLVER_H
#define	_STREAMMETADATARESOLVER_H

#include "common.h"
#include "protocols/timer/basetimerprotocol.h"

class StreamCapabilities;

class MetadataStats
: public Variant {
private:
	Variant _dummy;
public:
	VARIANT_COPY_CONSTRUCTORS(MetadataStats);
	VARIANT_GETSET(string, mediaFullPath, "");
	VARIANT_GETSET(uint64_t, openCount, 0);
	VARIANT_GETSET(uint64_t, servedBytesCount, 0);
	VARIANT_GETSET(Variant, lastAccessTime, _dummy);

	void Init() {
		mediaFullPath("");
		openCount(0);
		servedBytesCount(0);
	}
};

class Storage
: public Variant {
public:
	VARIANT_COPY_CONSTRUCTORS(Storage);
	VARIANT_GETSET(string, name, "");
	VARIANT_GETSET(string, mediaFolder, "");
	VARIANT_GETSET(string, metaFolder, "");
	VARIANT_GETSET(bool, enableStats, false);
	VARIANT_GETSET(bool, keyframeSeek, false);
	VARIANT_GETSET(string, description, "");
	VARIANT_GETSET(int32_t, clientSideBuffer, 0);
	VARIANT_GETSET(uint32_t, seekGranularity, 0);
	VARIANT_GETSET(bool, externalSeekGenerator, false);
};

class PublicMetadata
: public Variant {
public:
	VARIANT_COPY_CONSTRUCTORS(PublicMetadata);
	VARIANT_GETSET(double, startTimestamp, 0);
	VARIANT_GETSET(double, endTimestamp, 0);
	VARIANT_GETSET(double, duration, 0);
	VARIANT_GETSET(string, server, "");
	VARIANT_GETSET(double, bandwidth, 0);
	VARIANT_GETSET(uint32_t, audioFramesCount, 0);
	VARIANT_GETSET(uint32_t, videoFramesCount, 0);
	VARIANT_GETSET(uint32_t, totalFramesCount, 0);
	VARIANT_GETSET(uint64_t, fileSize, 0);
	VARIANT_GETSET(string, mediaFullPath, "");
};

class Metadata
: public Variant {
private:
	Storage _dummy1;
	PublicMetadata _dummy2;
public:
	VARIANT_COPY_CONSTRUCTORS(Metadata);
	VARIANT_GETSET(string, originalStreamName, "");
	VARIANT_GETSET(string, type, "");
	VARIANT_GETSET(string, completeFileName, "");
	VARIANT_GETSET(string, fileName, "");
	VARIANT_GETSET(string, extension, "");
	VARIANT_GETSET(string, computedCompleteFileName, "");
	VARIANT_GETSET(string, mediaFullPath, "");
	VARIANT_GETSET(Storage&, storage, _dummy1);
	VARIANT_GETSET(string, hash, "");
	VARIANT_GETSET(string, seekFileFullPath, "");
	VARIANT_GETSET(string, metaFileFullPath, "");
	VARIANT_GETSET(string, statsFileFullPath, "");
	VARIANT_GETSET(PublicMetadata&, publicMetadata, _dummy2);
};

enum StatsOperation {
	STATS_OPERATION_INCREMENT_OPEN_COUNT = 0,
	STATS_OPERATION_INCREMENT_SERVED_BYTES_COUNT
};

class StreamMetadataResolver;

class StreamMetadataResolverTimer
: public BaseTimerProtocol {
private:
	StreamMetadataResolver *_pResolver;

	struct statsOperation {
		string mediaFullPath;
		string statsFile;
		StatsOperation operation;
		uint64_t value;
	};
	vector<statsOperation> _operations1;
	vector<statsOperation> _operations2;
	vector<statsOperation> *_pAcceptQueue;
	vector<statsOperation> *_pWorkQueue;
public:
	StreamMetadataResolverTimer(StreamMetadataResolver *pResolver);
	virtual ~StreamMetadataResolverTimer();
	void ResetStreamManager();

	virtual bool TimePeriodElapsed();

	void EnqueueOperation(string &mediaFullPath, string &statsFile,
			StatsOperation operation, uint64_t value);
};

class StreamMetadataResolver {
private:
	StreamCapabilities *_pCapabilities;
	map<string, Storage *> _storagesByMediaFolder;
	vector<Storage *> _storagesByOrder;
	map<string, pair<double, uint64_t> > _badFiles;
	map<string, bool> _badStatsFiles;
	Variant _storages;
	bool _silence;
	uint32_t _statsTimerId;
	string _recordedStreamsStorage;
public:
	StreamMetadataResolver();
	virtual ~StreamMetadataResolver();

	/*!
	 * @brief used to silence all logging messages. Useful for external seek
	 * generators who needs to be silent
	 */
	void SetSilence(bool silence);

	/*!
	 * @brief This will initialize the StreamMetadataResolver instance
	 *
	 * @param configuration - the storage node from the configuration file
	 */
	bool Initialize(Variant &configuration);

	/*!
	 * @brief This will initialize a storage location
	 *
	 * @param name - the name of the storage location
	 *
	 * @param config - all the parameters associated with the storage
	 *
	 * @param result - If successful, it will contain the normalized storage
	 *
	 * @retuns true on success, false on errors
	 */
	bool InitializeStorage(string name, Variant &config, Storage &result);

	/*!
	 * @brief removes a storage from the list of storages
	 *
	 * @param mediaFolder - the media folder for the storage that needs to be
	 * removed
	 *
	 * @param removedStorage - if the storage is found and removed, it is also
	 * copied into this variable. useful to see what was the exact storage that
	 * was removed
	 */
	void RemoveStorage(string mediaFolder, Storage &removedStorage);

	/*!
	 * @brief This function can be used to resolve complete metadata for a
	 * a given stream name
	 *
	 * @param streamName - This designates the stream for which the metadata
	 * needs to be constructed. It has the following forms:<br>
	 * 1. streamName<br>
	 * 2. /path/to/file.mp4 or c:\\path\\to\\file.mp4<br>
	 * The main difference between them is that #1 is a real stream name while
	 * #2 is a path to a file. If the streamName is not a real absolute path
	 * than is considered to be a stream name and adheres to the Adobe naming
	 * convention (mp4:test.mp4, mp3:test, test, etc)
	 *
	 * @param result - This is where all the metadata will be deposited. It can
	 * be pre-populated with other custom data/keys. However, if a naming
	 * collision is encountered (existing key), the existing key will be
	 * overwritten. It is best to adopt the sub-nodes technique to store custom
	 * properties into this rather than have them stored directly into the root
	 *
	 * @returns returns true if the operation succeeded or false if it failed
	 *
	 * @discussion This function will always try to deep-resolve the metadata.
	 * That means it will also create *.seek and *.meta files if needed. Those
	 * files are not created if they are newer than the media files. If this
	 * files are older than the media files, that means the media file is a
	 * different file and the seek/meta files are outdated.
	 */
	bool ResolveMetadata(string streamName, Metadata &result);

	/*!
	 * @brief This function can be used to resolve stream name metadata for a
	 * a given stream name
	 *
	 * @param streamName - This designates the stream for which the metadata
	 * needs to be constructed. It has the following forms:<br>
	 * 1. streamName<br>
	 * 2. /path/to/file.mp4 or c:\\path\\to\\file.mp4<br>
	 * The main difference between them is that #1 is a real stream name while
	 * #2 is a path to a file. If the streamName is not a real absolute path
	 * than is considered to be a stream name and adheres to the Adobe naming
	 * convention (mp4:test.mp4, mp3:test, test, etc)
	 *
	 * @param result - This is where all the metadata will be deposited. It can
	 * be pre-populated with other custom data/keys. However, if a naming
	 * collision is encountered (existing key), the existing key will be
	 * overwritten. It is best to adopt the sub-nodes technique to store custom
	 * properties into this rather than have them stored directly into the root
	 *
	 * @returns returns true if the operation succeeded or false if it failed
	 */
	bool ResolveStreamName(string streamName, Metadata &result);

	/*!
	 * @brief this function cycles over all files in all storages and builds the
	 * appropriate seek/meta files
	 */
	void GenerateMetaFiles();

	/*!
	 * @brief this will return the full list of storages
	 */
	Variant &GetAllStorages();

	/*
	 * @brief this will increment the open count on the stats
	 */
	void UpdateStats(string mediaFullPath, string statsFile,
			StatsOperation operation, uint64_t value);

	/*!
	 * @brief returns the default location for the recorded streams.
	 * To be used inside protocols like RTMP where publishing is also requiring
	 * recording on the fly
	 */
	string GetRecordedStreamsStorage();
private:
	void SetRecordedSteramsStorage(Variant &value);
	void UpdateStats(MetadataStats &stats, StatsOperation operation, uint64_t value);
	void EnqueueStatsOperation(string &mediaFullPath, string &statsFile,
			StatsOperation operation, uint64_t value);
	bool ResolveStorage(Metadata &result);
	bool ComputeSeekMetaPaths(Metadata &result);
	bool ComputeSeekMeta(Metadata &result);
	bool LoadSeekMeta(Metadata &result);
	void DeleteAllMetaFiles(Metadata &result);
};


#endif	/* _STREAMMETADATARESOLVER_H */
