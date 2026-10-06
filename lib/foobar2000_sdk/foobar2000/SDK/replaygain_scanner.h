#pragma once

#include "replaygain.h"

//! Container of ReplayGain scan results from one or more tracks.
class replaygain_result : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(replaygain_result, service_base);
public:
	//! Retrieves the gain value, in dB.
	virtual float get_gain() = 0;
	//! Retrieves the peak value, normalized to 0..1 range (audio_sample value).
	virtual float get_peak() = 0;
	//! Merges ReplayGain scan results from different tracks. Merge results from all tracks in an album to get album gain/peak values. \n
	//! This function returns a newly created replaygain_result object. Existing replaygain_result objects remain unaltered.
	virtual replaygain_result::ptr merge(replaygain_result::ptr other) = 0;

	replaygain_info make_track_info() {
		replaygain_info ret = replaygain_info_invalid; ret.m_track_gain = this->get_gain(); ret.m_track_peak = this->get_peak(); return ret;
	}
};

//! Instance of a ReplayGain scanner. \n
//! Use @c replaygain_scanner_entry::instantiate() to create a @c replaygain_scanner object; see @c replaygain_scanner_entry for more info. \n
//! Typical use: call @c process_chunk() with each chunk read from your track, call @c finalize() to obtain results for this track and reset @c replaygain_scanner's state for scanning another track; to obtain album gain/peak values, merge results (`replaygain_result::merge`) from all tracks. \n
class replaygain_scanner : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(replaygain_scanner, service_base);
public:
	//! Processes a PCM chunk. \n
	//! May throw @c exception_io_data if the chunk contains something that can't be processed properly.
	virtual void process_chunk(const audio_chunk & chunk) = 0;
	//! Finalizes the scanning process; resets scanner's internal state and returns results for the track we've just scanned. \n
	//! After calling @c finalize(), scanner's state is undefined and should be used no longer; create a new one to scan next track.
	virtual replaygain_result::ptr finalize() = 0;
};


//! Entrypoint class for instantiating @c replaygain_scanner objects. \n
//! This service is OPTIONAL; it's available from foobar2000 0.9.5.3 up but only if the ReplayGain Scanner component is installed. \n
//! It is recommended that you use replaygain_scanner like this: \n
//! `replaygain_scanner_entry::ptr theAPI;` \n
//! `if (replaygain_scanner_entry::tryGet(theAPI)) {` \n
//!     `myInstance = theAPI->instantiate();` \n
//! `} else {` \n
//!     `no foo_rgscan installed - complain/fail/etc` \n
//! `}` \n
//! Note that @c replaygain_scanner_entry::get() is provided for convenience - it WILL crash without ReplayGain Scanner present. Use it only after prior checks.
class replaygain_scanner_entry : public service_base {
	FB2K_MAKE_SERVICE_COREAPI(replaygain_scanner_entry);
public:
	//! Instantiates a @c replaygain_scanner object.
	virtual replaygain_scanner::ptr instantiate() = 0;

	//! Helper; uses @c replaygain_scanner_entry_v2 if available; see @c replaygain_scanner_entry_v2.
	replaygain_scanner::ptr instantiate( uint32_t flags );
};

//! \since 1.4
//! This service is OPTIONAL; it's available from foobar2000 v1.4 up but only if the ReplayGain Scanner component is installed. \n
//! Use @c tryGet() to instantiate - @c get() only after prior verification of availability.
class replaygain_scanner_entry_v2 : public replaygain_scanner_entry {
	FB2K_MAKE_SERVICE_COREAPI_EXTENSION(replaygain_scanner_entry_v2, replaygain_scanner_entry)
public:
	static constexpr uint32_t 
		flagScanPeak = 1 << 0,
		flagScanGain = 1 << 1,
		flagsDefault = flagScanPeak | flagScanGain;

	//! Extended instantiation method. \n
	//! Allows you to declare which parts of the scanning process are relevant for you
	//! so irrelevant parts of the processing can be skipped.
	//! For an example, if you don't care about the peak, specify only @c flagScanGain -
	//! as peak scan while normally cheap may be very expensive with extreme oversampling specified by user.
	virtual replaygain_scanner::ptr instantiate(uint32_t flags) = 0;
};

//! \since 2.26
class replaygain_algorithm_description : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(replaygain_algorithm_description, service_base)
public:
	virtual GUID getGuid() = 0;
	virtual fb2k::stringRef getName() = 0;
};

class dsp_chain_config;

//! \since 2.26
class replaygain_scanner_setup : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(replaygain_scanner_setup, service_base)
public:
	//! Retrieve description of algorithm being used.
	virtual replaygain_algorithm_description::ptr getAlgorithm() = 0;
	//! Set description of algorithm being used.
	virtual void setAlgorithm(replaygain_algorithm_description::ptr) = 0;
	//! Retrieve boolean value indicating whether true peak scanning is enabled.
	virtual bool getTruePeak() = 0;
	//! Set boolean value indicating whether true peak scanning is enabled.
	virtual void setTruePeak(bool) = 0;
	//! Retrieve true peak oversample ratio, if ratio-based true peak is enabled.
	//! @returns True peak oversample ratio, 1 if ratio-based true peak isn't enabled.
	virtual unsigned getTruePeakRatio() = 0;
	//! Set true peak oversample ratio. Clears true peak chain, sets true peak mode to on if value is >1.
	virtual void setTruePeakRatio(unsigned) = 0;
	//! Retrieves DSP chain used for true peak, if used.
	//! @returns A boolean value indicating whether chain was retrieved.
	virtual bool getTruePeakChain(dsp_chain_config&) = 0;
	//! Sets DSP chain used for true peak. Overrides true peak ratio, sets true peak mode to on if chain isn't empty.
	virtual void setTruePeakChain(dsp_chain_config const&) = 0;
	
	virtual replaygain_scanner::ptr instantiate(uint32_t flags = replaygain_scanner_entry_v2::flagsDefault) = 0;
};

//! \since 2.26
class replaygain_scanner_entry_v3 : public replaygain_scanner_entry_v2 {
	FB2K_MAKE_SERVICE_COREAPI_EXTENSION(replaygain_scanner_entry_v3, replaygain_scanner_entry_v2)
public:
	//! @returns Newly created @c replaygain_scanner_setup::ptr configured with global options.
	virtual replaygain_scanner_setup::ptr setup() = 0;
	//! @returns array of @c replaygain_algorithm_description objects
	virtual fb2k::arrayRef listAlgorithms() = 0;
};

//! Internal service introduced in 1.5, to access high level user settings of ReplayGain scanner component. No guarantees about compatibility. May be changed or removed at any time.
class replaygain_scanner_config : public service_base {
	FB2K_MAKE_SERVICE_COREAPI(replaygain_scanner_config);
public:
	virtual void get_album_pattern( pfc::string_base & out ) = 0;
	virtual uint64_t get_read_size_bytes() = 0;
};

#ifdef FOOBAR2000_DESKTOP
//! \since 1.4
//! A class for applying gain to compressed audio packets such as MP3 or AAC. \n
//! Implemented by ReplayGain Scanner for common formats. May be extended to allow @c foo_rgscan to manipulate more different codecs.
class replaygain_alter_stream : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE(replaygain_alter_stream, service_base)
public:
	//! @returns The amount to which all adjustments are quantized for this format. Essential for caller to be able to correctly prevent clipping.
	virtual float get_adjustment_step( ) = 0;
	//! Sets the adjustment in decibels. Note that the actual applied adjustment will be quantized with nearest-rounding to a multiple of @c get_adjustment_step() value.
	virtual void set_adjustment( float deltaDB ) = 0;
	//! Passes the first frame playload. This serves as a hint and may be safely ignored for most formats. However in some cases - MP3 vs MP2 in particular - you do not know what exact format you're dealing with until you've examined the first frame. \n
	//! If you're calling this service, always feed the first frame before calling @c get_adjustment_step().
	virtual void on_first_frame( const void * frame, size_t bytes ) = 0;
	//! Applies gain to the frame. \n
	//! May throw @c exception_io_data if the frame payload is corrupted and cannot be altered. The user will be informed about bad frame statistics, however the operation will continue until EOF.
	virtual void alter_frame( void * frame, size_t bytes ) = 0;
};

//! \since 1.4
//! Entrypoint class for instantiating @c replaygain_alter_stream. Walk with @c  service_enum_t<> to find one that supports specific format.
class replaygain_alter_stream_entry : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(replaygain_alter_stream_entry);
public:
	//! @returns Newly created @c replaygain_alter_stream object. Null if this format is not supported by this implementation.
	//! Arguments as per  @c packet_decoder::g_open().
	virtual replaygain_alter_stream::ptr open(const GUID & p_owner, size_t p_param1, const void * p_param2, size_t p_param2size ) = 0;
};
#endif
