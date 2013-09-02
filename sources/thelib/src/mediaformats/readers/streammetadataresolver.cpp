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

#include "mediaformats/readers/streammetadataresolver.h"
#include "mediaformats/readers/mediaframe.h"
#include "mediaformats/readers/mp4/mp4document.h"
#include "mediaformats/readers/flv/flvdocument.h"
#include "mediaformats/readers/mp3/mp3document.h"
#include "mediaformats/readers/ts/tsdocument.h"
#include "streaming/streamcapabilities.h"
#include "protocols/protocolmanager.h"

#define SILENCE_FATAL(...) if(!_silence) FATAL(__VA_ARGS__)
#define SILENCE_WARN(...) if(!_silence) WARN(__VA_ARGS__)

StreamMetadataResolverTimer::StreamMetadataResolverTimer(
		StreamMetadataResolver *pResolver) {
	_pResolver = pResolver;
	_pAcceptQueue = &_operations1;
	_pWorkQueue = &_operations2;
}

StreamMetadataResolverTimer::~StreamMetadataResolverTimer() {
}

void StreamMetadataResolverTimer::ResetStreamManager() {
	_pResolver = NULL;
}

bool StreamMetadataResolverTimer::TimePeriodElapsed() {

	FOR_VECTOR(*_pWorkQueue, i) {
		if (_pResolver == NULL)
			continue;
		//		FINEST("mediaFullPath: %s statsFile: %s operation: %d value: %"PRIu64,
		//				STR((*_pWorkQueue)[i].mediaFullPath),
		//				STR((*_pWorkQueue)[i].statsFile),
		//				(*_pWorkQueue)[i].operation,
		//				(*_pWorkQueue)[i].value);
		_pResolver->UpdateStats((*_pWorkQueue)[i].mediaFullPath,
				(*_pWorkQueue)[i].statsFile,
				(*_pWorkQueue)[i].operation,
				(*_pWorkQueue)[i].value);
	}

	_pWorkQueue->clear();

	vector<statsOperation> *pTemp = _pWorkQueue;
	_pWorkQueue = _pAcceptQueue;
	_pAcceptQueue = pTemp;

	return true;
}

void StreamMetadataResolverTimer::EnqueueOperation(string &mediaFullPath,
		string &statsFile, StatsOperation operation, uint64_t value) {
	statsOperation so = {mediaFullPath, statsFile, operation, value};
	_pAcceptQueue->push_back(so);
}

StreamMetadataResolver::StreamMetadataResolver() {
	_pCapabilities = new StreamCapabilities();
	_silence = false;
}

StreamMetadataResolver::~StreamMetadataResolver() {
	if (_pCapabilities != NULL) {
		delete _pCapabilities;
		_pCapabilities = NULL;
	}
	_storagesByMediaFolder.clear();

	FOR_VECTOR(_storagesByOrder, i) {
		delete _storagesByOrder[i];
	}
	_storagesByOrder.clear();
	StreamMetadataResolverTimer *pTemp = (StreamMetadataResolverTimer *) ProtocolManager::GetProtocol(_statsTimerId);
	if (pTemp != NULL) {
		pTemp->ResetStreamManager();
		pTemp->EnqueueForDelete();
	}
}

void StreamMetadataResolver::SetSilence(bool silence) {
	_silence = silence;
}

bool StreamMetadataResolver::Initialize(Variant &configuration) {
	Storage dummy;

	bool hasTimers = true;
	if (configuration.HasKeyChain(V_BOOL, false, 1, "hasTimers")) {
		hasTimers = (bool)configuration.GetValue("hasTimers", false);
		configuration.RemoveKey("hasTimers", false);
	}

	FOR_MAP(configuration, string, Variant, i) {
		if ((lowerCase(MAP_KEY(i)) == "recordedstreamsstorage")
				&&(MAP_VAL(i) == V_STRING)) {
			SetRecordedSteramsStorage(MAP_VAL(i));
		} else {
			if (!InitializeStorage(MAP_KEY(i), MAP_VAL(i), dummy)) {
				WARN("Storage failed to initialize storage %s", STR(MAP_KEY(i)));
			}
		}
	}

	if (hasTimers) {
		StreamMetadataResolverTimer *pTimer = new StreamMetadataResolverTimer(this);
		_statsTimerId = pTimer->GetId();
		pTimer->EnqueueForTimeEvent(1);
	}
	return true;
}

bool StreamMetadataResolver::InitializeStorage(string name, Variant &config,
		Storage &result) {
	if (!config.HasKeyChain(V_STRING, false, 1, CONF_APPLICATION_MEDIAFOLDER)) {
		WARN("mediaFolder has incorrect type");
		return false;
	}

	//mediaFolder
	string tempString = (string) config.GetValue(CONF_APPLICATION_MEDIAFOLDER, false);
	string mediaFolder = normalizePath(tempString, "");
	if (mediaFolder == "") {
		WARN("mediaFolder not found: %s", STR(tempString));
		return false;
	}
	if (mediaFolder[mediaFolder.size() - 1] != PATH_SEPARATOR)
		mediaFolder += PATH_SEPARATOR;
	if (MAP_HAS1(_storagesByMediaFolder, mediaFolder)) {
		WARN("mediaFolder %s already present in one of the storages", STR(mediaFolder));
		return false;
	}

	//metaFolder
	string metaFolder = "";
	if (config.HasKeyChain(V_STRING, false, 1, CONF_APPLICATION_METAFOLDER)) {
		metaFolder = (string) config.GetValue(CONF_APPLICATION_METAFOLDER, false);
	} else {
		metaFolder = "";
	}

	if (metaFolder == "") {
		WARN("meta folder for storage %s not specified. seek/meta files will be created inside the media folder",
				STR(name));
		metaFolder = mediaFolder;
	} else {
		tempString = normalizePath(metaFolder, "");
		if (tempString == "") {
			WARN("Specified metaFolder not found: %s", STR(metaFolder));
			return false;
		}
		metaFolder = tempString;
	}
	if (metaFolder[metaFolder.size() - 1] != PATH_SEPARATOR)
		metaFolder += PATH_SEPARATOR;

	//enableStats
	bool enableStats = false;
	if (config.HasKeyChain(V_BOOL, false, 1, CONF_APPLICATION_ENABLESTATS)) {
		enableStats = (bool) config.GetValue(CONF_APPLICATION_ENABLESTATS, false);
	}

	//keyframeSeek
	bool keyframeSeek = true;
	if (config.HasKeyChain(V_BOOL, false, 1, CONF_APPLICATION_KEYFRAMESEEK)) {
		keyframeSeek = (bool) config.GetValue(CONF_APPLICATION_KEYFRAMESEEK, false);
	}

	//description
	string description = "";
	if (config.HasKeyChain(V_STRING, false, 1, CONF_APPLICATION_DESCRIPTION)) {
		description = (string) config.GetValue(CONF_APPLICATION_DESCRIPTION, false);
	}

	//clientSideBuffer
	uint32_t clientSideBuffer = 0;
	if (config.HasKeyChain(_V_NUMERIC, false, 1, CONF_APPLICATION_CLIENTSIDEBUFFER)) {
		clientSideBuffer = (uint32_t) config.GetValue(CONF_APPLICATION_CLIENTSIDEBUFFER, false);
	}
	if (clientSideBuffer != 0xffffffff) {
		if (clientSideBuffer == 0)
			clientSideBuffer = 15;
		if (clientSideBuffer > 3600)
			clientSideBuffer = 3600;
	}

	//seekGranularity
	int32_t seekGranularity = 0;
	if (config.HasKeyChain(_V_NUMERIC, false, 1, CONF_APPLICATION_SEEKGRANULARITY)) {
		seekGranularity = (int32_t) ((double) config.GetValue(CONF_APPLICATION_SEEKGRANULARITY, false)*1000.0);
	}
	if (seekGranularity < 100)
		seekGranularity = 1000;
	if (seekGranularity > 300000)
		seekGranularity = 300000;

	//externalSeekGenerator
	bool externalSeekGenerator = false;
	if (config.HasKeyChain(V_BOOL, false, 1, CONF_APPLICATION_EXTERNALSEEKGENERATOR)) {
		externalSeekGenerator = (bool) config.GetValue(CONF_APPLICATION_EXTERNALSEEKGENERATOR, false);
	}


	Storage *pStorage = new Storage();
	//pStorage->originalConfigNode(config);
	pStorage->name(name);
	pStorage->mediaFolder(mediaFolder);
	pStorage->metaFolder(metaFolder);
	pStorage->enableStats(enableStats);
	pStorage->keyframeSeek(keyframeSeek);
	pStorage->description(description);
	pStorage->clientSideBuffer(clientSideBuffer);
	pStorage->seekGranularity(seekGranularity);
	pStorage->externalSeekGenerator(externalSeekGenerator);

	//FINEST("\n%s", STR(pStorage->ToString()));
	_storagesByMediaFolder[mediaFolder] = pStorage;
	ADD_VECTOR_END(_storagesByOrder, pStorage);

	vector<string> files;
	if (!listFolder(pStorage->metaFolder(), files, true, false, true)) {
		WARN("Unable to list folder %s", STR(pStorage->metaFolder()));
	}

	FOR_VECTOR(files, i) {
		//X.stats.tmp
		uint32_t minSize = 1 + MEDIA_TYPE_STATS_LEN + 4;
		if (files[i].size()<(1 + minSize))
			continue;
		string fullFile = files[i];
		if (fullFile.substr(fullFile.size() - minSize) == "."MEDIA_TYPE_STATS".tmp") {
			string statsFile = fullFile.substr(0, fullFile.size() - 4);
			WARN("Moving opened stats file %s -> %s", STR(fullFile), STR(statsFile));
			moveFile(fullFile, statsFile);
		}
	}

	result = *pStorage;
	_storages.Reset();
	return true;
}

void StreamMetadataResolver::RemoveStorage(string mediaFolder, Storage &removedStorage) {
	mediaFolder = normalizePath(mediaFolder, "");
	if (mediaFolder != "") {
		if (mediaFolder[mediaFolder.size() - 1] != PATH_SEPARATOR)
			mediaFolder += PATH_SEPARATOR;
	}

	FOR_VECTOR_ITERATOR(Storage *, _storagesByOrder, i) {
		if (VECTOR_VAL(i)->mediaFolder() == mediaFolder) {
			removedStorage = *VECTOR_VAL(i);
			_storagesByOrder.erase(i);
			_storagesByMediaFolder.erase(removedStorage.mediaFolder());
			_storages.Reset();
			return;
		}
	}
}

bool StreamMetadataResolver::ResolveMetadata(string streamName, Metadata &result) {
	//0. Do we have any storage?
	if (_storagesByOrder.size() == 0) {
		SILENCE_FATAL("No valid storages defined");
		return false;
	}

	//1. resolve the streamName first and extract the absolute path and type
	if (!ResolveStreamName(streamName, result)) {
		SILENCE_FATAL("Stream name %s not found", STR(streamName));
		return false;
	}

	//2. Get the storage
	if (!ResolveStorage(result)) {
		SILENCE_FATAL("Stream name %s not found in any storage", STR(streamName));
		return false;
	}

	//3. Compute seek/meta file paths
	if (!ComputeSeekMetaPaths(result)) {
		SILENCE_FATAL("Unable to compute seek/meta file paths for %s", STR(streamName));
		return false;
	}

	//4. Compute seek/meta files
	if (!ComputeSeekMeta(result)) {
		SILENCE_FATAL("Unable to compute seek/meta files %s", STR(streamName));
		return false;
	}

	//5. Done
	return true;
}

bool StreamMetadataResolver::ResolveStreamName(string streamName, Metadata &result) {
	//1. Store the original stream request
	result.originalStreamName(streamName);

	//2. it must be non-empty
	if (streamName.size() < 1)
		return false;

	//3. Declare all the variables. general format is:
	/*
	 * streamName=[<type>:]<completeFileName>
	 * completeFileName=<fileName>[.extension]
	 *
	 * For the type/extension, there is the following fall-back:
	 *
	 * if type is empty than type is extension
	 * if type is still empty than type is defaulted to flv
	 * if extension is empty than extension is type
	 *
	 * After all this were properly read, a validation is made on the type so
	 * it will have one of the following values:
	 *
	 * MEDIA_TYPE_MOV
	 * MEDIA_TYPE_MP4
	 * MEDIA_TYPE_F4V
	 * MEDIA_TYPE_F4V
	 * MEDIA_TYPE_M4A
	 * MEDIA_TYPE_M4V
	 * MEDIA_TYPE_MP3
	 * MEDIA_TYPE_FLV
	 * MEDIA_TYPE_LIVE_OR_FLV
	 *
	 * If type is not one of those values, the metadata will not be supported
	 * and the file is declared as not found. First 6 types will set the
	 * final value of type to mp4. Last 7th type (mp3) will not change the type
	 * Last 2 types will set the final value of type to flv
	 * */
	string type = "";
	string completeFileName = "";
	string fileName = "";
	string extension = "";
	string computedCompleteFileName = "";

	if (isAbsolutePath(streamName)) {
		type = "";
		completeFileName = streamName;
	} else {
		string::size_type colonPos = streamName.find(':');
		if (colonPos != string::npos) {
			type = lowerCase(streamName.substr(0, colonPos));
			completeFileName = streamName.substr(colonPos + 1);
		} else {
			type = "";
			completeFileName = streamName;
		}
	}

	//4. Compute the values
	string::size_type dotPos = completeFileName.rfind('.');
	if (dotPos != string::npos) {
		fileName = completeFileName.substr(0, dotPos);
		extension = completeFileName.substr(dotPos + 1);
	} else {
		fileName = completeFileName;
		extension = "";
	}
	if (type == "")
		type = lowerCase(extension);
	if (type == "")
		type = MEDIA_TYPE_FLV;
	if (extension == "") {
		computedCompleteFileName = fileName + "." + type;
		extension = type;
	} else {
		computedCompleteFileName = completeFileName;
	}

	//5. if the path is absolute, check for its existence
	if (isAbsolutePath(computedCompleteFileName)) {
		computedCompleteFileName = normalizePath(computedCompleteFileName, "");
		if (!fileExists(computedCompleteFileName)) {
			return false;
		}
	}

	//6. Check the type and see if it is supported
	bool supported = false;
	if ((type == MEDIA_TYPE_MOV)
			|| (type == MEDIA_TYPE_MP4)
			|| (type == MEDIA_TYPE_F4V)
			|| (type == MEDIA_TYPE_F4V)
			|| (type == MEDIA_TYPE_M4A)
			|| (type == MEDIA_TYPE_M4V)) {
		type = MEDIA_TYPE_MP4;
		supported = true;
	} else if ((type == MEDIA_TYPE_MP3)
			|| (type == MEDIA_TYPE_FLV)
			|| (type == MEDIA_TYPE_TS)) {
		supported = true;
	} else if (type == MEDIA_TYPE_LIVE_OR_FLV) {
		type = MEDIA_TYPE_FLV;
		supported = true;
	}

	//	FINEST("originalStreamName: `%s`; type: `%s`; completeFileName: `%s`; fileName: `%s`; extension: `%s`; computedCompleteFileName: `%s`; supported: %d",
	//			STR(result.originalStreamName()),
	//			STR(type),
	//			STR(completeFileName),
	//			STR(fileName),
	//			STR(extension),
	//			STR(computedCompleteFileName),
	//			supported);

	//7. If the type is not supported, than bail out
	if (!supported)
		return false;

	//8. Store the information
	result.type(type);
	result.completeFileName(completeFileName);
	result.fileName(fileName);
	result.extension(extension);
	result.computedCompleteFileName(computedCompleteFileName);
	result.mediaFullPath("");
	result.hash("");
	result.seekFileFullPath("");
	result.metaFileFullPath("");
	result.statsFileFullPath("");

	//9. Done
	return true;
}

void StreamMetadataResolver::GenerateMetaFiles() {
	vector<string> files;
	Metadata temp;

	FOR_VECTOR(_storagesByOrder, i) {
		files.clear();
		if (!listFolder(_storagesByOrder[i]->mediaFolder(), files)) {
			WARN("Unable to list media folder %s", STR(_storagesByOrder[i]->mediaFolder()));
		}

		FOR_VECTOR(files, j) {
			temp.Reset();
			if (!ResolveMetadata(files[j], temp)) {
				SILENCE_WARN("Unable to generate metadata for file %s", STR(files[j]));
			}
		}
	}
}

Variant &StreamMetadataResolver::GetAllStorages() {
	if (_storages.MapSize() == 0) {

		FOR_VECTOR(_storagesByOrder, i) {
			_storages.PushToArray(*_storagesByOrder[i]);
		}
	}
	return _storages;
}

void StreamMetadataResolver::UpdateStats(string mediaFullPath, string statsFile,
		StatsOperation operation, uint64_t value) {
	if (statsFile == "")
		return;
	if (MAP_HAS1(_badStatsFiles, statsFile))
		return;
	string tempStatsFile = statsFile + ".tmp";
	if (fileExists(statsFile)) {
		if (!moveFile(statsFile, tempStatsFile)) {
			EnqueueStatsOperation(mediaFullPath, statsFile, operation, value);
			return;
		}
		MetadataStats stats;
		stats.Init();
		if (!Variant::DeserializeFromXmlFile(tempStatsFile, stats)) {
			string damagedStatsFile = statsFile + "." + generateRandomString(8);
			WARN("Stats file damaged: %s. Will move it to: %s", STR(statsFile), STR(damagedStatsFile));
			if (!moveFile(tempStatsFile, damagedStatsFile)) {
				WARN("Unable to move stats file %s -> %s", STR(tempStatsFile),
						STR(damagedStatsFile));
				_badStatsFiles[statsFile] = true;
				return;
			}
			stats.Reset();
			stats.Init();
			stats.mediaFullPath(mediaFullPath);
			if (!stats.SerializeToXmlFile(tempStatsFile)) {
				WARN("Unable to write stats file: %s. Will permanently give up on this file",
						STR(tempStatsFile));
				_badStatsFiles[statsFile] = true;
				return;
			}
			if (!moveFile(tempStatsFile, statsFile)) {
				WARN("Unable to move stats file %s -> %s", STR(tempStatsFile),
						STR(statsFile));
				_badStatsFiles[statsFile] = true;
				return;
			}
		}
		UpdateStats(stats, operation, value);
		if (!stats.SerializeToXmlFile(tempStatsFile)) {
			WARN("Unable to update stats file %s", STR(tempStatsFile));
			_badStatsFiles[statsFile] = true;
		}
		if (!moveFile(tempStatsFile, statsFile)) {
			WARN("Unable to move stats file %s -> %s", STR(tempStatsFile), STR(statsFile));
			_badStatsFiles[statsFile] = true;
		}
	} else {
		if (!fileExists(tempStatsFile)) {
			MetadataStats stats;
			stats.Init();
			stats.mediaFullPath(mediaFullPath);
			if (!stats.SerializeToXmlFile(tempStatsFile)) {
				WARN("Unable to write stats file: %s. Will permanently give up on this file",
						STR(tempStatsFile));
				_badStatsFiles[statsFile] = true;
				return;
			}
			if (!moveFile(tempStatsFile, statsFile)) {
				WARN("Unable to move stats file %s -> %s", STR(tempStatsFile),
						STR(statsFile));
				_badStatsFiles[statsFile] = true;
				return;
			}
			UpdateStats(mediaFullPath, statsFile, operation, value);
		} else {
			EnqueueStatsOperation(mediaFullPath, statsFile, operation, value);
		}
	}
}

string StreamMetadataResolver::GetRecordedStreamsStorage() {
	return _recordedStreamsStorage;
}

void StreamMetadataResolver::SetRecordedSteramsStorage(Variant &value) {
	if (value != V_STRING)
		return;
	string strValue = (string) value;
	string normalizedValue = normalizePath(strValue, "");
	if (normalizedValue == "") {
		WARN("the location value for the recorded streams is incorrect: %s", STR(strValue));
		return;
	}
	if (normalizedValue[normalizedValue.length() - 1] != PATH_SEPARATOR)
		normalizedValue += PATH_SEPARATOR;
	File testFile;
	string testFileName = normalizedValue + generateRandomString(16);
	testFile.SuppressLogErrorsOnInit();
	if (!testFile.Initialize(testFileName, FILE_OPEN_MODE_TRUNCATE)) {
		WARN("the location value for the recorded streams is not write-able: %s", STR(normalizedValue));
		return;
	}
	testFile.Close();
	deleteFile(testFileName);
	_recordedStreamsStorage = normalizedValue;
}

void StreamMetadataResolver::UpdateStats(MetadataStats &stats,
		StatsOperation operation, uint64_t value) {
	switch (operation) {
		case STATS_OPERATION_INCREMENT_OPEN_COUNT:
		{
			value = stats.openCount() + 1;
			stats.openCount(value);
			stats.lastAccessTime(Variant::Now());
			break;
		}
		case STATS_OPERATION_INCREMENT_SERVED_BYTES_COUNT:
		{
			value += stats.servedBytesCount();
			stats.servedBytesCount(value);
			break;
		}
		default:
		{
			WARN("Stats operation %"PRIu32" not implemented", (uint32_t) operation);
			break;
		}
	}
}

void StreamMetadataResolver::EnqueueStatsOperation(string &mediaFullPath,
		string &statsFile, StatsOperation operation, uint64_t value) {
	StreamMetadataResolverTimer *pTimer = (StreamMetadataResolverTimer *)
			ProtocolManager::GetProtocol(_statsTimerId);
	if (pTimer == NULL)
		return;
	pTimer->EnqueueOperation(mediaFullPath, statsFile, operation, value);
}

bool StreamMetadataResolver::ResolveStorage(Metadata &result) {
	//1. get the computed complete file name. By complete we understand
	//that the extension is also computed if missing. This is not the
	//full path yet.
	string computedCompleteFileName = result.computedCompleteFileName();

	//2. Is this an absolute path?
	bool isAbsolute = isAbsolutePath(computedCompleteFileName);

	//3. Search for the storage
	Storage *pStorage = NULL;
	string mediaFullPath = "";

	FOR_VECTOR(_storagesByOrder, i) {
		Storage *pTemp = _storagesByOrder[i];
		if (isAbsolute) {
			if (computedCompleteFileName.find(pTemp->mediaFolder()) == 0) {
				if (pStorage == NULL) {
					pStorage = pTemp;
					mediaFullPath = computedCompleteFileName;
				} else {
					if (pStorage->mediaFolder().size() < pTemp->mediaFolder().size()) {
						pStorage = pTemp;
					}
				}
			}
		} else {
			string temp = normalizePath(pTemp->mediaFolder(), computedCompleteFileName);
			if (temp == "")
				continue;
			mediaFullPath = temp;
			pStorage = pTemp;
			break;
		}
	}

	//4. If we found the storage, stor it in the metadata and exit
	if ((pStorage != NULL)&&(mediaFullPath != "")) {
		result.mediaFullPath(mediaFullPath);
		result.storage(*pStorage);
		return true;
	}

	//5. Storage not found
	return false;
}

bool StreamMetadataResolver::ComputeSeekMetaPaths(Metadata &result) {
	Storage &storage = result.storage();
	if (storage.metaFolder() == storage.mediaFolder()) {
		result.seekFileFullPath(result.mediaFullPath() + "." + MEDIA_TYPE_SEEK);
		result.metaFileFullPath(result.mediaFullPath() + "." + MEDIA_TYPE_META);
		if (storage.enableStats()) {
			result.statsFileFullPath(result.mediaFullPath() + "." + MEDIA_TYPE_STATS);
		}
	} else {
		string fingerprint = format("%s_%d_%"PRIu32,
				STR(result.mediaFullPath()),
				storage.keyframeSeek(),
				storage.seekGranularity());
		result.hash(lowerCase(md5(fingerprint, true)));
		File testFile;
		if (!testFile.Initialize(storage.metaFolder() + result.hash(), FILE_OPEN_MODE_TRUNCATE)) {
			SILENCE_FATAL("folder %s is not write-able by EMS", STR(storage.metaFolder()));
			return false;
		}
		testFile.Close();
		deleteFile(storage.metaFolder() + result.hash());
		result.seekFileFullPath(storage.metaFolder() + result.hash() + "." + MEDIA_TYPE_SEEK);
		result.metaFileFullPath(storage.metaFolder() + result.hash() + "." + MEDIA_TYPE_META);
		if (storage.enableStats()) {
			result.statsFileFullPath(storage.metaFolder() + result.hash() + "." + MEDIA_TYPE_STATS);
		}
	}
	return true;
}

bool StreamMetadataResolver::ComputeSeekMeta(Metadata &result) {
	//FINEST("result:\n%s", STR(result.ToString()));

	if (MAP_HAS1(_badFiles, result.mediaFullPath())) {
		File f;
		if (!f.Initialize(result.mediaFullPath(), FILE_OPEN_MODE_READ)) {
			return false;
		}
		uint64_t size = f.Size();
		f.Close();
		if ((_badFiles[result.mediaFullPath()].first < getFileModificationDate(result.mediaFullPath()))
				|| (_badFiles[result.mediaFullPath()].second != size)) {
			_badFiles.erase(result.mediaFullPath());
		} else {
			return false;
		}
	}

	//1. do we have the files already?
	if (LoadSeekMeta(result))
		return true;

	if (result.storage().externalSeekGenerator()) {
		WARN("Seek/meta files for file %s not yet generated", STR(result.mediaFullPath()));
		return false;
	}

	//1. get the type
	string type = result.type();

	//2. Instantiate the document
	BaseMediaDocument *pDocument = NULL;

	if (false) {
	}
#ifdef HAS_MEDIA_MP4
	else if (type == MEDIA_TYPE_MP4) {
		pDocument = new MP4Document(result);
	}
#endif /* HAS_MEDIA_MP4 */
#ifdef HAS_MEDIA_FLV
	else if (type == MEDIA_TYPE_FLV) {
		pDocument = new FLVDocument(result);
	}
#endif /* HAS_MEDIA_MP4 */
#ifdef HAS_MEDIA_MP3
	else if (type == MEDIA_TYPE_MP3) {
		pDocument = new MP3Document(result);
	}
#endif /* HAS_MEDIA_MP4 */

	if (pDocument == NULL) {
		SILENCE_FATAL("Media file type %s not supported", STR(type));
		return false;
	}

	INFO("Generate seek/meta files for `%s`", STR(result.mediaFullPath()));
	if (!pDocument->Process()) {
		FATAL("Unable to generate seek/meta files for %s", STR(result.mediaFullPath()));
		_badFiles[result.mediaFullPath()] = pair<double, uint64_t>(getFileModificationDate(result.mediaFullPath()),
				pDocument->GetMediaFile().Size());
		delete pDocument;
		pDocument = NULL;
	}

	if (pDocument != NULL) {
		delete pDocument;
		return true;
	} else {
		return false;
	}
}

bool StreamMetadataResolver::LoadSeekMeta(Metadata &result) {
	if ((!fileExists(result.seekFileFullPath()))
			|| (!fileExists(result.metaFileFullPath())))
		return false;
	if ((getFileModificationDate(result.metaFileFullPath()) < getFileModificationDate(result.mediaFullPath()))
			|| (getFileModificationDate(result.seekFileFullPath()) < getFileModificationDate(result.mediaFullPath()))) {
		WARN("seek/meta files for media %s obsolete. Delete them", STR(result.mediaFullPath()));
		DeleteAllMetaFiles(result);
		return false;
	}

	PublicMetadata publicMetadata;
	if (!PublicMetadata::DeserializeFromXmlFile(result.metaFileFullPath(), publicMetadata)) {
		WARN("meta file for media %s corrupted. Delete it and regenerate", STR(result.mediaFullPath()));
		DeleteAllMetaFiles(result);
		return false;
	}
	publicMetadata.RemoveKey("mediaFullPath");

	string temp = result.seekFileFullPath();
	if (!_pCapabilities->Deserialize(temp, NULL)) {
		WARN("seek file for media %s corrupted. Delete it and regenerate", STR(result.mediaFullPath()));
		DeleteAllMetaFiles(result);
		return false;
	}

	result.publicMetadata(publicMetadata);

	return true;
}

void StreamMetadataResolver::DeleteAllMetaFiles(Metadata &result) {
	if (fileExists(result.metaFileFullPath()))
		deleteFile(result.metaFileFullPath());
	if (fileExists(result.metaFileFullPath() + ".tmp"))
		deleteFile(result.metaFileFullPath() + ".tmp");

	if (fileExists(result.seekFileFullPath()))
		deleteFile(result.seekFileFullPath());
	if (fileExists(result.seekFileFullPath() + ".tmp"))
		deleteFile(result.seekFileFullPath() + ".tmp");

	if (fileExists(result.statsFileFullPath()))
		deleteFile(result.statsFileFullPath());
	if (fileExists(result.statsFileFullPath() + ".tmp"))
		deleteFile(result.statsFileFullPath() + ".tmp");
}
