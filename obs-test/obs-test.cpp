// obs-test.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <iostream>
#include <libobs/obs.h>
#include <windows.h>
#include <libavcodec/codec_id.h>
#include <string>


#define INPUT_AUDIO_SOURCE "wasapi_input_capture"
#define OUTPUT_AUDIO_SOURCE "wasapi_output_capture"
#define VIDEO_ENCODER_ID           AV_CODEC_ID_H264
#define VIDEO_ENCODER_NAME         "libx264"
#define RECORD_OUTPUT_FORMAT       "mp4"
#define RECORD_OUTPUT_FORMAT_MIME  "video/mp4"
#define VIDEO_FPS            45
#define VIDEO_ENCODER_ID           AV_CODEC_ID_H264
#define AUDIO_BITRATE 128 
#define VIDEO_BITRATE 150 
#define OUT_WIDTH  1920
#define OUT_HEIGHT 1080

char capture_type = {};
char source = {};

enum SOURCE_CHANNELS {
	SOURCE_CHANNEL_TRANSITION,
	SOURCE_CHANNEL_AUDIO_OUTPUT,
	SOURCE_CHANNEL_AUDIO_OUTPUT_2,
	SOURCE_CHANNEL_AUDIO_INPUT,
	SOURCE_CHANNEL_AUDIO_INPUT_2,
	SOURCE_CHANNEL_AUDIO_INPUT_3,
};


static void AddSource(void* _data, obs_scene_t* scene)
{
	obs_source_t* source = (obs_source_t*)_data;
	obs_scene_add(scene, source);
	obs_source_release(source);
}


static inline bool HasAudioDevices(const char* source_id)
{
	const char* output_id = source_id;
	obs_properties_t* props = obs_get_source_properties(output_id);
	size_t count = 0;

	if (!props)
		return false;

	obs_property_t* devices = obs_properties_get(props, "device_id");
	if (devices)
		count = obs_property_list_item_count(devices);

	obs_properties_destroy(props);

	return count != 0;
}

void ResetAudioDevice(const char* sourceId, const char* deviceId,
	const char* deviceDesc, int channel)
{
	bool disable = deviceId && strcmp(deviceId, "disabled") == 0;
	obs_source_t* source;
	obs_data_t* settings;

	source = obs_get_output_source(channel);
	if (source) {
		if (disable) {
			obs_set_output_source(channel, nullptr);
		}
		else {
			settings = obs_source_get_settings(source);
			const char* oldId = obs_data_get_string(settings, "device_id");
			if (strcmp(oldId, deviceId) != 0)
			{
				obs_data_set_string(settings, "device_id",
					deviceId);
				obs_source_update(source, settings);
			}
			obs_data_release(settings);
		}
		obs_source_release(source);

	}
	else if (!disable) {
		settings = obs_data_create();
		obs_data_set_string(settings, "device_id", deviceId);
		source = obs_source_create(sourceId, deviceDesc, settings, nullptr);
		obs_data_release(settings);

		obs_set_output_source(channel, source);
		obs_source_release(source);
	}
}

bool InitAudio()
{
	obs_audio_info ai = {};
	ai.samples_per_sec = 48000;
	ai.speakers = SPEAKERS_STEREO;
	if (!obs_reset_audio(&ai))
	{
		return false;
	}
	return true;
}

int base_width = {};
int base_height = {};
int out_width = {};
int out_height = {};
int fps = {};

bool InitVedio()
{
	printf("Please enter the base length.\n");
	scanf_s(" %d", &base_width);
	printf("Please enter the base width\n");
	scanf_s(" %d", &base_height);
	printf("Please enter the output length.\n");
	scanf_s(" %d", &out_width);
	printf("Please enter the output width.\n");
	scanf_s(" %d", &out_height);
	printf("Please enter the fps\n");
	scanf_s(" %d", &fps);

	obs_video_info ovi = {};
	ovi.graphics_module = "libobs-d3d11.dll";
	ovi.graphics_module = "libobs-opengl.dll";
	ovi.base_width = base_width;
	ovi.base_height = base_height;
	ovi.output_width = out_width;
	ovi.output_height = out_height;
	ovi.output_format = VIDEO_FORMAT_NV12;
	ovi.fps_num = fps;
	ovi.fps_num = 60;
	ovi.fps_den = 1;
	ovi.adapter = 0;  
	ovi.colorspace = VIDEO_CS_709;
	ovi.range = VIDEO_RANGE_PARTIAL;
	ovi.gpu_conversion = true;
	ovi.scale_type = OBS_SCALE_BICUBIC;
	if (0 != obs_reset_video(&ovi))
	{
		return false;
	}
	return true;
}

bool InitObs()
{
	if (!obs_startup("en-US", nullptr, nullptr))
	{
		return false;
	}

	if (!InitAudio())
	{
		return false;
	}

	if (!InitVedio())
	{
		return false;
	}	

	obs_load_all_modules();
	obs_log_loaded_modules();
	obs_post_load_modules();

	return true;
}

void SetSysAudio()
{
	bool hasDesktopAudio = HasAudioDevices(OUTPUT_AUDIO_SOURCE);
	if (hasDesktopAudio)
	{
		ResetAudioDevice(OUTPUT_AUDIO_SOURCE, "default", "Default Desktop Audio", SOURCE_CHANNEL_AUDIO_OUTPUT);
	}
}


void SetMicAudio()
{
	bool hasInputAudio = HasAudioDevices(INPUT_AUDIO_SOURCE);
	if (hasInputAudio)
	{
		ResetAudioDevice(INPUT_AUDIO_SOURCE, "default", "Default Mic/Aux", SOURCE_CHANNEL_AUDIO_INPUT);
	}
}

bool InitSource(const std::string& source_type,int capture_type)
{
	obs_set_output_source(SOURCE_CHANNEL_TRANSITION, nullptr);
	obs_set_output_source(SOURCE_CHANNEL_AUDIO_OUTPUT, nullptr);
	obs_set_output_source(SOURCE_CHANNEL_AUDIO_INPUT, nullptr);

	obs_source_t* fadeTransition = nullptr;

	size_t idx = 0;
	const char* id;

	/* automatically add transitions that have no configuration (things
	 * such as cut/fade/etc) */
	while (obs_enum_transition_types(idx++, &id)) {
		const char* name = obs_source_get_display_name(id);

		if (!obs_is_source_configurable(id)) {
			obs_source_t* tr = obs_source_create_private(id, name, NULL);

			if (strcmp(id, "fade_transition") == 0)
				fadeTransition = tr;
		}
	}

	if (!fadeTransition)
	{
		return false;
	}

	obs_set_output_source(SOURCE_CHANNEL_TRANSITION, fadeTransition);
	obs_source_release(fadeTransition);

	auto scene = obs_scene_create("MyScene");
	if (!scene)
	{
		return false;
	}

	obs_source_t* s = obs_get_output_source(SOURCE_CHANNEL_TRANSITION);
	obs_transition_set(s, obs_scene_get_source(scene));
	obs_source_release(s);
	
	obs_source_t* capture_source = {};
	if (source_type == "monitor")
	{
		capture_source = obs_source_create("monitor_capture", "Computer_Monitor_Capture", NULL, nullptr);
	}
	if (source_type == "window")
	{
		capture_source = obs_source_create("window_capture", "Computer_Window_Capture", NULL, nullptr);
	}

	if (capture_source)
	{
		obs_scene_atomic_update(scene, AddSource, capture_source);
	}

	obs_data_t* setting = obs_data_create();
	obs_data_t* curSetting = obs_source_get_settings(capture_source);

	obs_data_apply(setting, curSetting);
	obs_data_release(curSetting); 

	auto properties = obs_source_properties(capture_source);
	obs_property_t* property = obs_properties_first(properties);

	std::vector<std::string> capture_name;

	const char* name = {};
	/*d3d11*/
	while (property)
	{
		name = obs_property_name(property);

		if (std::string(name) == "window")
		{
			size_t count = obs_property_list_item_count(property);
			printf("Please select the window to be recorded.\n");
			for (size_t i = 0; i < count; i++)
			{
				auto item_name = obs_property_list_item_string(property, i);
				capture_name.emplace_back(item_name);
				printf("%d.%s\n", i, item_name);
			}
		}

		if (std::string(name) == "monitor_id")
		{
			size_t count = obs_property_list_item_count(property);
			printf("Please select the monitor to be recorded.\n");
			for (size_t i = 0; i < count; i++)
			{
				auto item_name = obs_property_list_item_name(property, i);
				auto item_string = obs_property_list_item_string(property, i);
				capture_name.emplace_back(item_string);
				printf("%d.%s\n", i, item_name);
			}
		}

		obs_property_next(&property);
	}

	/*OpenGL todo*/

	int select_type = 0;
	scanf_s("%d", &select_type);

	if (source_type == "monitor")
	{
		obs_data_set_string(setting, "monitor_id", capture_name[select_type].c_str());
	}
	if (source_type == "window")
	{
		obs_data_set_string(setting, source_type.c_str(), capture_name[select_type].c_str());
	}
	
	obs_data_set_int(setting, "method", capture_type);

	obs_source_update(capture_source, setting);
	obs_data_release(setting);

	return true;
}


obs_output_t* fileOutput = {};

bool InitOutput(const std::string& encode_type)
{
	obs_data_t* settings = obs_data_create();

	//ffmpeg
	fileOutput = obs_output_create("ffmpeg_output", "adv_ffmpeg_output", nullptr, nullptr);

	auto vencoder = obs_video_encoder_create("obs_x264", "advanced_video_stream", nullptr, nullptr);
	auto aencoder = obs_audio_encoder_create("ffmpeg_aac", "adv_stream_audio", nullptr, 0, nullptr);

	obs_encoder_release(vencoder);
	obs_encoder_release(aencoder);

	obs_data_set_int(settings, "gop_size", 250);
	obs_data_set_int(settings, "video_bitrate", 36000);
	obs_data_set_int(settings, "video_encoder_id", 0);
	obs_data_set_int(settings, "audio_bitrate", 160);
	obs_data_set_int(settings, "audio_encoder_id", 0);

	obs_data_set_string(settings, "url", "C:\\Users\\Admin\\Desktop\\obs\\play\\123.mp4");
	obs_output_set_mixer(fileOutput, 1);
	obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	obs_output_update(fileOutput, settings);

	obs_data_release(settings);
	
	//obs_data_set_string(settings, "url", "C:\\Users\\Admin\\Desktop\\obs\\play\\123.mp4");
	//obs_data_set_string(settings, "format_name", RECORD_OUTPUT_FORMAT);
	//obs_data_set_string(settings, "format_mime_type", RECORD_OUTPUT_FORMAT_MIME);
	//obs_data_set_string(settings, "muxer_settings", "movflags=faststart");
	//obs_data_set_int(settings, "gop_size", VIDEO_FPS * 10);
	//obs_data_set_string(settings, "video_encoder", VIDEO_ENCODER_NAME);
	//obs_data_set_int(settings, "video_encoder_id", VIDEO_ENCODER_ID);

	//if (VIDEO_ENCODER_ID == AV_CODEC_ID_H264)
	//	obs_data_set_string(settings, "video_settings", "profile=main x264-params=crf=18");
	//else if (VIDEO_ENCODER_ID == AV_CODEC_ID_FLV1)
	//	obs_data_set_int(settings, "video_bitrate", VIDEO_BITRATE);

	//obs_data_set_int(settings, "audio_bitrate", AUDIO_BITRATE);
	//obs_data_set_string(settings, "audio_encoder", "aac");
	//obs_data_set_int(settings, "audio_encoder_id", AV_CODEC_ID_AAC);
	//obs_data_set_string(settings, "audio_settings", NULL);
	//obs_data_set_int(settings, "scale_width", OUT_WIDTH);
	//obs_data_set_int(settings, "scale_height", OUT_HEIGHT);

	//obs_output_set_mixer(fileOutput, 1);
	//obs_output_set_media(fileOutput, obs_get_video(), obs_get_audio());
	//obs_output_update(fileOutput, settings);

	//obs_data_release(settings);

	return true;
}

bool InitSimpleOutput(const std::string& encode_type)
{
	fileOutput = obs_output_create("ffmpeg_muxer", "simple_file_output", NULL, NULL);
	obs_data_t* settings = obs_data_create();	

	std::string path = "C:\\Users\\Admin\\Desktop\\obs\\play\\";
	path += std::to_string(GetTickCount64()/1000);

	path += "-" + encode_type;

	path += source == 'a' ? "-monitr" : "-window";

	if (source == 'a')
	{
		path += capture_type == 'a' ? "-DXGI" : "-WGC";
	}
	if (source == 'b')
	{
		path += capture_type == 'a' ? "-BitBlt" : "-WGC";
	}

	path += "-resolution" + std::to_string(base_width) + "x" +std::to_string(base_height);
	path += "-fps" + std::to_string(fps);

	printf("Please enter the cqp\n");
	int cqp = 0;
	scanf_s(" %d", &cqp);
	path += "-cqp" + std::to_string(cqp);

	printf("Please select the recording format.\na.mkv\nb.mp4\nc.flv\n");
	char extension = {};
	scanf_s(" %c", &extension);
	if(extension == 'a')
		path += ".mkv";
	if (extension == 'b')
		path += ".mp4";
	if (extension == 'c')
		path += ".flv";


	obs_data_set_string(settings, "path", path.c_str());

	obs_encoder_t* vencoder = nullptr;
	//ffmpeg_nvenc
	if (encode_type == "nvenc")
	{
		//h264
		vencoder = obs_video_encoder_create("jim_nvenc", "simple_video_recording", nullptr, nullptr);
		
		//hevc
		//vencoder = obs_video_encoder_create("jim_hevc_nvenc", "simple_video_recording", nullptr, nullptr);
	}
	if (encode_type == "obs_x264")
	{
		vencoder = obs_video_encoder_create("obs_x264", "test_x264", nullptr, nullptr);
	}
	
	auto aencoder = obs_audio_encoder_create("ffmpeg_aac", "test_aac", nullptr, 0, nullptr);

	auto vencoder_settings = obs_data_create();
	auto aencoder_settings = obs_data_create();

	if (encode_type == "nvenc")
	{
		obs_data_set_string(vencoder_settings, "rate_control", "CQP");
		obs_data_set_int(vencoder_settings, "bitrate", 0);
		obs_data_set_int(vencoder_settings, "cqp", cqp);
	}

	if (encode_type == "obs_x264")
	{
		obs_data_set_string(vencoder_settings, "rate_control", "CRF");
		obs_data_set_int(vencoder_settings, "bitrate", 0);
		obs_data_set_int(vencoder_settings, "crf", 16);
	}
	
	obs_encoder_update(vencoder, vencoder_settings);
	obs_encoder_update(aencoder, aencoder_settings);

	obs_output_update(fileOutput, settings);

	obs_encoder_set_video(vencoder, obs_get_video());
	obs_encoder_set_audio(aencoder, obs_get_audio());

	obs_output_set_video_encoder(fileOutput, vencoder);
	obs_output_set_audio_encoder(fileOutput, aencoder,0);

	obs_data_release(settings);
	return true;
}

obs_output_t* replayBuffer = {};

bool InitReplayOutput(const std::string & encode_type)
{
	replayBuffer = obs_output_create("replay_buffer","ReplayBuffer",nullptr, nullptr);
	obs_data_t* settings = obs_data_create();

	printf("Please enter the output path for the playback video.\n");
	char tmp_path[256] = {};
	scanf_s(" %s", tmp_path, 256);
	obs_data_set_string(settings, "directory", tmp_path);
	obs_data_set_string(settings, "format", "Replay %CCYY-%MM-%DD %hh-%mm-%ss");
	printf("Please select the recording format.\na.mkv\nb.mp4\nc.flv\n");
	char extension = {};
	scanf_s(" %c", &extension);
	if (extension == 'a')
		obs_data_set_string(settings, "extension", "mkv");
	if (extension == 'b')
		obs_data_set_string(settings, "extension", "mp4");
	if (extension == 'c')
		obs_data_set_string(settings, "extension", "flv");

	obs_data_set_int(settings, "max_time_sec", 5);
	obs_data_set_int(settings, "max_size_mb", 512);

	obs_encoder_t* vencoder = nullptr;
	if (encode_type == "nvenc")
	{
		vencoder = obs_video_encoder_create("jim_nvenc", "simple_video_recording", nullptr, nullptr);
	}

	if (encode_type == "obs_x264")
	{
		vencoder = obs_video_encoder_create("obs_x264", "test_x264", nullptr, nullptr);
	}

	auto aencoder = obs_audio_encoder_create("ffmpeg_aac", "test_aac", nullptr, 0, nullptr);

	auto vencoder_settings = obs_data_create();
	auto aencoder_settings = obs_data_create();

	obs_encoder_update(vencoder, vencoder_settings);
	obs_encoder_update(aencoder, aencoder_settings);
	obs_output_update(replayBuffer, settings);

	obs_encoder_set_video(vencoder, obs_get_video());
	obs_encoder_set_audio(aencoder, obs_get_audio());

	obs_output_set_video_encoder(replayBuffer, vencoder);
	obs_output_set_audio_encoder(replayBuffer, aencoder, 0);

	obs_data_release(settings);
	return true;
}


int main()
{
	printf("Please select the function\na. Record video\nb. Playback function\n");
	char fun_type;
	scanf_s(" %c", &fun_type);

	printf("Please select the decoder\na. Hardware decoding, requires NV graphics card\nb. Software decoding\n");
	char encode_type;
	scanf_s(" %c", &encode_type);

	printf("Please select the source\na. Display capture\nb. Window capture\n");
	scanf_s(" %c", &source);
	
	if (source == 'a')
	{
		printf("Please select the capture method\na. DXGI\nb. WGC\n");
		scanf_s(" %c", &capture_type);
		if (capture_type == 'a')
		{
			SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
		}
	}
	if (source == 'b')
	{
		printf("Please select the capture method\na. BitBlt (does not support capturing transparent windows)\nb. WGC\n");
		scanf_s(" %c", &capture_type);
	}

	if (!InitObs())
	{
		printf("initobs error\n");
		Sleep(10000);
		return false;
	}

	if (!InitSource(source == 'a' ? "monitor" : "window" , capture_type == 'a'?1:2))
	{
		printf("InitSource error\n");
		Sleep(10000);
		return false;
	}

	SetSysAudio();
	SetMicAudio();

	if (fun_type == 'a')
	{
		if (!InitSimpleOutput(encode_type == 'a' ? "nvenc" : "obs_x264"))
		{
			printf("InitSimpleOutput error\n");
			Sleep(10000);
			return false;
		}

		auto ret = obs_output_start(fileOutput);

		while (true)
		{
			printf("Please select an operation\na. Stop\n");
			char type;
			scanf_s(" %c", &type);
			if (type == 'a')
			{
				obs_output_stop(fileOutput);
				break;
			}
		}
	}

	if (fun_type == 'b')
	{
		if (!InitReplayOutput(encode_type == 'a' ? "nvenc" : "obs_x264"))
		{
			printf("InitReplayOutput error\n");
			Sleep(10000);
			return false;
		}

		auto ret = obs_output_start(replayBuffer);
		while (true)
		{
			printf("Please select an operation\na. Playback\nb. Stop\n");
			char type;
			scanf_s(" %c", &type);
			if (type == 'a')
			{
				ret = obs_output_active(replayBuffer);
				calldata_t cd = { 0 };
				proc_handler_t* ph =obs_output_get_proc_handler(replayBuffer);
				proc_handler_call(ph, "save", &cd);
				calldata_free(&cd);
			}
			if (type == 'b')
			{
				obs_output_stop(replayBuffer);
				break;
			}
		}
	}

	return 0;
}
