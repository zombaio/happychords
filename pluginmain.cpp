//@	{
//@	"targets":
//@		[{
//@		 "name":"happychords2.so","type":"lib_dynamic"
//@		,"dependencies":
//@			[
//@				 {"ref":"lv2plug","rel":"external"}
//@			]
//@		}]
//@	}


#include <maike/targetinclude.hpp>
#include MAIKE_TARGET(plugindescriptor.hpp)
#include "blob.hpp"
#include "voice.hpp"
#include "sawtooth.hpp"
#include <lv2plug/lv2plug.hpp>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <cmath>
#include <utility>
#include <algorithm>
#include <random>

using namespace Happychords;


static constexpr auto waveform=Happychords::sawtooth();

class PRIVATE Engine:public LV2Plug::Plugin<PluginDescriptor>
	{
	public:
		Engine(double fs,const char* path_bundle
			,LV2Plug::FeatureDescriptor&& features):m_features(features)
			,m_fs(fs),voice_adsr(fs,1e-3f,1e-1f,0.5f,1e-1)
			{}

		void process(size_t n_frames) noexcept;

	private:
		LV2Plug::FeatureDescriptor m_features;
		double m_fs;
		Adsr::Params voice_adsr;
		ArrayStatic<int8_t,128> keys;
		ArrayStatic<Voice<waveform.size()>,8> voices;
		ArrayStatic<float,64> buffer_in;
		ArrayStatic<ArrayStatic<Framepair,64>,3> bufftemp;

		void generate(size_t n_frames) noexcept;
		void processEvents() noexcept;
		void voiceActivate(int8_t key,float amplitude) noexcept;
	};

static float frequencyGet(float key) noexcept
	{
	return 440.0f*std::exp2((key - 69.0f)/12.0f);
	}

void Engine::voiceActivate(int8_t key,float amplitude) noexcept
	{
	static_assert(voices.size()!=0,"No voices?");
	auto voice_min=0;
	auto a_min=voices[0].amplitude();
	for(int k=1;k<static_cast<int>( voices.size() );++k)
		{
		auto a=voices[k].amplitude();
		if(a<a_min)
			{
			a_min=a;
			voice_min=k;
			}
		}
	voices[voice_min].start(frequencyGet(key)/m_fs,amplitude,voice_adsr,key);
	keys[key]=voice_min;
	}

void Engine::processEvents()noexcept
	{
	auto midi_in=portmap().get<Ports::MIDI_IN>();
	if(midi_in==nullptr)
		{return;}

	LV2_Atom_Event* ev = lv2_atom_sequence_begin(&(midi_in->body));
	while(!lv2_atom_sequence_is_end(&(midi_in->body)
		,midi_in->atom.size,ev))
		{
		if(ev->body.type==m_features.midi())
			{
			const uint8_t* const msg=(const uint8_t*)(ev + 1);
			switch(lv2_midi_message_type(msg))
				{
				case LV2_MIDI_MSG_NOTE_ON:
					voiceActivate(msg[1],msg[2]/127.0f);
					break;

				case LV2_MIDI_MSG_NOTE_OFF:
					voices[ keys[ msg[1] ] ].stop(msg[1]);
					break;

				default:
					break;
				}
			}
		ev = lv2_atom_sequence_next(ev);
		}
	}

static inline bool make_bool(float value)
	{return value>0.0f;}

void Engine::generate(size_t n_frames) noexcept
	{
	auto buffer_l=portmap().get<Ports::OUTPUT_L>();
	auto buffer_r=portmap().get<Ports::OUTPUT_R>();
	auto detune=portmap().get<Ports::VOICE_DETUNE>();
	auto suboct=make_bool( portmap().get<Ports::VOICE_SUBOCT>() );
	voice_adsr=Adsr::Params(m_fs
		,portmap().get<Ports::MAIN_ATTACK>()
		,portmap().get<Ports::MAIN_DECAY>()
		,portmap().get<Ports::MAIN_SUSTAIN>()
		,portmap().get<Ports::MAIN_RELEASE>());


	memset(bufftemp[2].begin(),0,sizeof(bufftemp)/bufftemp.size());
	while(n_frames!=0)
		{
		auto n=std::min(n_frames,bufftemp[0].size());
		for(size_t k=0;k<voices.size();++k)
			{
			if(voices[k].amplitude()>1e-3f || voices[k].started())
				{
				memset(bufftemp[0].begin(),0,sizeof(bufftemp)/bufftemp.size());
				voices[k].generate(waveform
					,detune,suboct,buffer_in.begin(),bufftemp[0].begin(),n);
				voices[k].modulate(bufftemp[0].begin(),voice_adsr,bufftemp[1].begin(),n);
				addToMix(bufftemp[1].begin(),bufftemp[2].begin(),n);
				}
			}
		

		demux(bufftemp[2].begin(),buffer_l,buffer_r,n);


		n_frames-=n;
		buffer_l+=n;
		buffer_r+=n;
		}
	}

void Engine::process(size_t n_frames) noexcept
	{
	processEvents();
	generate(n_frames);
	}


const LV2_Descriptor& LV2Plug::main()
	{
	return LV2Plug::descriptorGet<Engine>();
	}