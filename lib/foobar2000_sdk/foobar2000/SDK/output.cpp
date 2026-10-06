#include "foobar2000-sdk-pch.h"
#include "output.h"
#include "audio_chunk_impl.h"
#include "dsp.h"
#include "resampler.h"

pfc::string8 output_entry::get_device_name( const GUID & deviceID ) {
	pfc::string8 temp;
	if (!get_device_name(deviceID, temp)) temp = "[unknown device]";
	return temp;
}

namespace {
	class output_device_enum_callback_getname : public output_device_enum_callback {
	public:
		output_device_enum_callback_getname( const GUID & wantID, pfc::string_base & strOut ) : m_strOut(strOut), m_wantID(wantID) {}
		void on_device(const GUID & p_guid,const char * p_name,unsigned p_name_length) {
			if (!m_got && p_guid == m_wantID) {
				m_strOut.set_string(p_name, p_name_length);
				m_got = true;
			}
		}
		bool m_got = false;
		pfc::string_base & m_strOut;
		const GUID m_wantID;
	};

}

bool output_entry::get_device_name( const GUID & deviceID, pfc::string_base & out ) {
	output_device_enum_callback_getname cb(deviceID, out);
	this->enum_devices(cb);
	return cb.m_got;
}

bool output_entry::g_find( const GUID & outputID, output_entry::ptr & outObj ) {
	for (auto obj : enumerate()) {
		if (obj->get_guid() == outputID) {
			outObj = obj; return true;
		}
	}
	return false;
}

output_entry::ptr output_entry::g_try_find(const GUID& outputID) {
	for (auto obj : enumerate()) {
		if (obj->get_guid() == outputID) return obj;
	}
	return nullptr;
}
output_entry::ptr output_entry::g_find( const GUID & outputID ) {
	auto ret = g_try_find(outputID);
	if ( ret.is_empty() ) throw exception_output_module_not_found();
	return ret;
}


bool output::is_progressing_() {
    output_v4::ptr v4;
    if ( v4 &= this ) return v4->is_progressing();
    return true;
}

size_t output::update_v2_() {
    output_v4::ptr v4;
    if ( v4 &= this ) return v4->update_v2();
    bool bReady = false;
    this->update(bReady);
    return bReady ? SIZE_MAX : 0;
}

pfc::eventHandle_t output::get_trigger_event_() {
    output_v4::ptr v4;
    if ( v4 &= this ) return v4->get_trigger_event();
    return pfc::eventInvalid;
}

size_t output::process_samples_v2_(const audio_chunk& c) {
	output_v6::ptr v6;
	if (v6 &= this) return v6->process_samples_v2(c);
	this->process_samples(c);
	return c.get_sample_count();
}

void output::hint_source_(fb2k::objRef arg) {
	output_v7::ptr v7;
	if (v7 &= this) v7->hint_source(arg);
}

void output::flush_changing_track_() {
	output_v2::ptr v2;
	if (v2 &= this) v2->flush_changing_track();
	else this->flush();
}
unsigned output::get_forced_sample_rate_() {
	output_v3::ptr v3;
	if (v3 &= this) return v3->get_forced_sample_rate();
	else return 0;
}
unsigned output::get_forced_channel_mask_() {
	output_v5::ptr v5;
	if (v5 &= this) return v5->get_forced_channel_mask();
	else return 0;
}

audio_chunk::spec_t output::get_forced_spec() {
	audio_chunk::spec_t spec;
	spec.sampleRate = this->get_forced_sample_rate_();
	spec.chanMask = this->get_forced_channel_mask_();
	if (spec.chanMask) spec.chanCount = audio_chunk::g_count_channels(spec.chanMask);
	return spec;
}

output::latencyInfo_t output::get_latency_info_() {
	output_v8::ptr v8;
	if (v8 &= this) return v8->get_latency_info();
	double l = this->get_latency();
	if (this->is_progressing_()) return { l,l };
	else return { l,0 };
}

void output_impl::on_flush_internal() {
	m_eos = false; m_sent_force_play = false;
	m_incoming_ptr = 0;
	m_incoming.set_size(0);
}

void output_impl::flush() {
	on_flush_internal();
	on_flush();
}

void output_impl::flush_changing_track() {
	on_flush_internal();
	on_flush_changing_track();
}

void output_impl::update(bool & p_ready) {
    p_ready = update_v2() > 0;
}
size_t output_impl::update_v2() {

	// Clear preemptively
	m_can_write = 0;

	on_update();

	// No data yet, nothing to do, want data, can't signal how much because we don't know the format
	if (!m_incoming_spec.is_valid()) return SIZE_MAX;

	// First chunk in or format change
	if (m_incoming_spec != m_active_spec) {
		if (get_latency_samples() == 0) {
			// Ready for new format
			m_sent_force_play = false;
			open(m_incoming_spec);
			m_active_spec = m_incoming_spec;
		} else {
			// Previous format still playing, accept no more data
			this->send_force_play();
			return 0;
		}
	}
	
	// opened for m_incoming_spec stream

	// Store & update m_can_write on our end
	// We don't know what can_write_samples() actually does, could be expensive, avoid calling it repeatedly
	m_can_write = this->can_write_samples();

	if (m_incoming_ptr < m_incoming.get_size()) {
		t_size delta = pfc::min_t(m_incoming.get_size() - m_incoming_ptr, m_can_write * m_incoming_spec.chanCount);
		if (delta > 0) {
			PFC_ASSERT(!m_sent_force_play);
			write(audio_chunk_temp_impl(m_incoming.get_ptr() + m_incoming_ptr, delta / m_incoming_spec.chanCount, m_incoming_spec.sampleRate, m_incoming_spec.chanCount, m_incoming_spec.chanMask));
			m_incoming_ptr += delta;
			if (m_eos && this->queue_empty()) {
				this->send_force_play();
			}
		}

		m_can_write -= delta / m_incoming_spec.chanCount;
	}
    return m_can_write;
}

output_impl::latencySamples_t output_impl::get_latency_samples_v2() {
	auto s = this->get_latency_samples();
	return { .soft = s, .hard = is_progressing() ? s : 0 };
}

output_v8::latencyInfo_t output_impl::get_latency_info() {
	latencyInfo_t ret = {};
	if (m_incoming_spec.is_valid()) {
		ret.latency += audio_math::samples_to_time((m_incoming.get_size() - m_incoming_ptr) / m_incoming_spec.chanCount, m_incoming_spec.sampleRate);
	}
	if (m_active_spec.is_valid()) {
		auto s = this->get_latency_samples_v2();
		ret.latency += audio_math::samples_to_time(s.soft, m_active_spec.sampleRate);
		ret.hardQueued += audio_math::samples_to_time(s.hard, m_active_spec.sampleRate);
	}
	return ret;
}

double output_impl::get_latency() {
	double ret = 0;
	if (m_incoming_spec.is_valid()) {
		ret += audio_math::samples_to_time( (m_incoming.get_size() - m_incoming_ptr) / m_incoming_spec.chanCount, m_incoming_spec.sampleRate );
	}
	if (m_active_spec.is_valid()) {
		ret += audio_math::samples_to_time( get_latency_samples(), m_active_spec.sampleRate );
	}
	return ret;
}

void output_impl::force_play() {
	if ( m_eos ) return;
	m_eos = true;
	if (queue_empty()) send_force_play();
}
void output_impl::send_force_play() {
	if (m_sent_force_play) return;
	m_sent_force_play = true;
	this->on_force_play();
}

static void spec_sanity(audio_chunk::spec_t const& spec) {
	if (!spec.is_valid()) pfc::throw_exception_with_message< exception_io_data >("Invalid audio stream specifications");
}

size_t output_impl::process_samples_v2(const audio_chunk& p_chunk) {
	PFC_ASSERT(queue_empty());
	PFC_ASSERT(!m_eos);
	const auto spec = p_chunk.get_spec();
	if (m_incoming_spec != spec) {
		spec_sanity(spec);
		m_incoming_spec = spec;
		return 0;
	}

	auto in = p_chunk.get_sample_count();
	if (in > m_can_write) in = m_can_write;
	if (in > 0) {
		write(audio_chunk_partial_ref(p_chunk, 0, in));
		m_can_write -= in;
	}
	return in;
}

void output_impl::process_samples(const audio_chunk & p_chunk) {
	PFC_ASSERT(queue_empty());
	PFC_ASSERT( !m_eos );
	const auto spec = p_chunk.get_spec();
	size_t taken = 0;
	if (m_incoming_spec == spec) {
		// Try bypassing intermediate buffer
		taken = this->process_samples_v2(p_chunk);
		if (taken == p_chunk.get_sample_count()) return; // all written, success
		taken *= spec.chanCount;
	} else {
		spec_sanity(spec);
		m_incoming_spec = spec;
	}
	// Queue what's left for update() to eat later
	m_incoming.set_data_fromptr(p_chunk.get_data() + taken, p_chunk.get_used_size() - taken);
	m_incoming_ptr = 0;
}

void output_v3::get_injected_dsps( dsp_chain_config & dsps ) {
	dsps.remove_all();
}

size_t output_v4::update_v2() {
    bool bReady = false;
    update(bReady);
    return bReady ? SIZE_MAX : 0;
}

uint32_t output_entry::get_config_flags_compat() {
	uint32_t ret = get_config_flags();
	if ((ret & (flag_low_latency | flag_high_latency)) == 0) {
		// output predating flag_high_latency + flag_low_latency
		// if it's old foo_out_upnp, report high latency, otherwise low latency.
		static const GUID guid_foo_out_upnp = { 0x9900b4f6, 0x8431, 0x4b0a, { 0x95, 0x56, 0xa7, 0xfc, 0xb9, 0x5b, 0x74, 0x3 } };
		if (this->get_guid() == guid_foo_out_upnp) ret |= flag_high_latency;
		else ret |= flag_low_latency;
	}
	return ret;
}

bool output_entry::is_high_latency() {
	return (this->get_config_flags_compat() & flag_high_latency) != 0;
}

bool output_entry::is_low_latency() {
	return (this->get_config_flags_compat() & flag_low_latency) != 0;
}

// {EEEB07DE-C2C8-44c2-985C-C85856D96DA1}
const GUID output_id_null = 
{ 0xeeeb07de, 0xc2c8, 0x44c2, { 0x98, 0x5c, 0xc8, 0x58, 0x56, 0xd9, 0x6d, 0xa1 } };

// {D41D2423-FBB0-4635-B233-7054F79814AB}
const GUID output_id_default =
{ 0xd41d2423, 0xfbb0, 0x4635, { 0xb2, 0x33, 0x70, 0x54, 0xf7, 0x98, 0x14, 0xab } };

#ifdef FOOBAR2000_DESKTOP

outputCoreConfig_t outputCoreConfig_t::defaults() {
	outputCoreConfig_t cfg = {};
	cfg.m_bitDepth = 16;
	cfg.m_buffer_length = 1.0;
	cfg.m_output = output_id_default;
	// remaining fields nulled by {}
	return cfg;
}
namespace {
	class output_device_list_callback_impl : public output_device_list_callback {
	public:
		void onDevice( const char * fullName, const GUID & output, const GUID & device ) {
			f(fullName, output, device);
		}
		std::function< void ( const char*, const GUID&, const GUID&) > f;
	};

	class output_config_change_callback_impl : public output_config_change_callback {
	public:
		void outputConfigChanged() {
			f();
		}
		std::function<void () > f;
	};
}
void output_manager_v2::listDevices( std::function< void ( const char*, const GUID&, const GUID&) > f ) {
	output_device_list_callback_impl cb; cb.f = f;
	this->listDevices( cb );
}

service_ptr output_manager_v2::addCallback( std::function<void() > f ) {
	output_config_change_callback_impl * obj = new output_config_change_callback_impl();
	obj->f = f;
 	this->addCallback( obj ); 
	return fb2k::callOnRelease( [obj, selfRef = fb2k::wrap_service_ptr(this)] {
		selfRef->removeCallback( obj ); delete obj;
	} );
}

void output_manager_v2::addCallbackPermanent( std::function<void()> f ) {
	output_config_change_callback_impl * obj = new output_config_change_callback_impl();
	obj->f = f;
	addCallback( obj );
}

void output_manager::getCoreConfig(outputCoreConfig_t& out) { 
	getCoreConfig(&out, sizeof(out)); 
}

outputCoreConfig_t output_manager::getCoreConfig() { 
	outputCoreConfig_t ret; getCoreConfig(ret); return ret; 
}
#endif // FOOBAR2000_DESKTOP

bool output_entry::supports_user_bit_depth(const GUID& deviceID) {
    // Can't use device_property_() wrapper, we handle this differently if interface not implemented
	output_entry_v4::ptr v4;
	if (v4 &= this) return v4->device_property(deviceID, output_props::user_bit_depth, 0, nullptr, 0) != 0;
	return (this->get_config_flags() & flag_needs_bitdepth_config) != 0;
}

size_t output_entry::device_property_(const GUID& device, const GUID& prop, size_t arg1, void* arg2, size_t arg2size) {
    output_entry_v4::ptr v4;
    size_t ret = 0;
    if (v4 &= this) {
        ret = v4->device_property(device, prop, arg1, arg2, arg2size);
    }
    return ret;
}

uint32_t output_entry::default_bit_depth(const GUID& deviceID) {
    uint32_t ret = (uint32_t) this->device_property_(deviceID, output_props::default_bit_depth, 0, nullptr, 0);
    if ( ret == 0 ) ret = 16;
	return ret;
}

size_t output_v6::process_samples_v2(const audio_chunk& arg) {
	this->process_samples(arg); return arg.get_sample_count();

}

void output_devices_notify::toggle_notify(bool bNotify) {
    for( auto walk : output_entry::enumerate() ) {
        output_entry_v3::ptr v3;
        if ( v3 &= walk ) {
            if (bNotify) v3->add_notify(this);
            else v3->remove_notify(this);
        }
    }
}
