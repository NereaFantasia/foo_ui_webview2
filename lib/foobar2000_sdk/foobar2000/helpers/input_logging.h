#pragma once


class input_logging : public input_stubs {
public:
	input_logging() {
		set_logger(nullptr);
	}

	event_logger_recorder::ptr log_record( std::function<void () > f ) { 
		auto rec = event_logger_recorder::create();
		{
			pfc::vartoggle_t toggle( m_logger, rec );
			f();
		}
		return rec;
	}
	
	void set_logger( event_logger::ptr logger ) {
		if (!logger) logger = &fb2k::noLogger;
		m_logger = logger;
	}
	bool loggerValid() const noexcept { return m_logger.get() != &fb2k::noLogger; }
	event_logger::ptr loggerOrNull() const noexcept { return loggerValid() ? m_logger : nullptr; }
protected:
	event_logger::ptr m_logger;
};

#define FB2K_INPUT_LOG_STATUS(X) FB2K_LOG_STATUS(m_logger, X)
#define FB2K_INPUT_LOG_WARNING(X) FB2K_LOG_WARNING(m_logger, X)
#define FB2K_INPUT_LOG_ERROR(X) FB2K_LOG_ERROR(m_logger, X)
