#include "audio_chunk_impl.h"
static int dsp_selftest() {
    dsp_chunk_list_impl list;
    audio_chunk_impl myChunk; myChunk.set_srate( 44100 ); myChunk.set_channels(2); myChunk.set_silence_seconds(0.5);
    list.add_chunk( &myChunk );
    PFC_ASSERT( list.get_count() == 1 );
    myChunk.set_silence_seconds(1);
    list.add_chunk( &myChunk );
    PFC_ASSERT( list.get_count() == 2 );
    PFC_ASSERT( list.get_duration() == 1.5 );

    {
        double total = 0;
        size_t num = 0;
        for( auto & ck : list ) {
            PFC_ASSERT( ck.get_sample_rate() == 44100 );
            total += ck.get_duration();
            num ++;
        }
        PFC_ASSERT( total == 1.5 );
        PFC_ASSERT( num == 2 );
    }
    
    list.remove_by_idx(0);
    PFC_ASSERT( list.get_duration() == 1 );
    PFC_ASSERT( list.get_count() == 1 );
    
    {
        size_t num = 0;
        for( auto & ck : list ) { std::ignore = ck; ++num; }
        PFC_ASSERT( num == 1 );
    }
    
    list.add_chunk( &myChunk );
    PFC_ASSERT( list.get_count() == 2 );
    PFC_ASSERT( list.get_duration() == 2 );

    return 0;
};
static int dsp_selftest_trigger = dsp_selftest();
