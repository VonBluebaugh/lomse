//---------------------------------------------------------------------------------------
// This file is part of the Lomse library.
// Lomse is copyrighted work (c) 2010-2018. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice, this
//      list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright notice, this
//      list of conditions and the following disclaimer in the documentation and/or
//      other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
// SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
// DAMAGE.
//
// For any comment, suggestion or feature request, please contact the manager of
// the project at cecilios@users.sourceforge.net
//---------------------------------------------------------------------------------------

#define LOMSE_INTERNAL_API
#include "lomse_score_player.h"

#include "lomse_midi_table.h"
#include "lomse_internal_model.h"
#include "lomse_injectors.h"
#include "lomse_events.h"
#include "lomse_interactor.h"
#include "lomse_player_gui.h"
#include "lomse_metronome.h"
#include "lomse_logger.h"

#include <algorithm>    //max(), min()
#include <ctime>        //clock()


namespace lomse
{

//---------------------------------------------------------------------------------------
ScorePlayer::ScorePlayer(LibraryScope& libScope, MidiServerBase* pMidi)
    : m_libScope(libScope)
    , m_pThread(nullptr)
    , m_pMidi(pMidi)
    , m_fPaused(false)
    , m_fShouldStop(false)
    , m_fPlaying(false)
    , m_fPostEvents(true)
    , m_fQuit(false)
    , m_fFinalEventSent(false)
    , m_pScore(nullptr)
    , m_pTable(nullptr)
    , m_MtrChannel(9)
    , m_MtrInstr(0)
    , m_MtrTone1(60)
    , m_MtrTone2(77)
    , m_fVisualTracking(false)
    , m_nMM(60)
    , m_pInteractor(nullptr)
    , m_pPlayerGui(nullptr)
    , m_pMtr(nullptr)
{
}

//---------------------------------------------------------------------------------------
ScorePlayer::~ScorePlayer()
{
    stop();
}

//---------------------------------------------------------------------------------------
void ScorePlayer::load_score(ImoScore* pScore, PlayerGui* pPlayerGui,
                             int metronomeChannel, int metronomeInstr,
                             int tone1, int tone2)
{
    stop();

    m_pScore = pScore;
    m_pPlayerGui = pPlayerGui;
    m_MtrChannel = metronomeChannel;
    m_MtrInstr = metronomeInstr;
    m_MtrTone1 = tone1;
    m_MtrTone2 = tone2;
    m_pMtr = m_pPlayerGui->get_metronome();

    m_pTable = m_pScore->get_midi_table();
}

//---------------------------------------------------------------------------------------
void ScorePlayer::play(bool fVisualTracking, long nMM, Interactor* pInteractor)
{
	//play all the score

    m_fVisualTracking = fVisualTracking;
    m_nMM = nMM;
    m_pInteractor = pInteractor;

    int evStart = m_pTable->get_first_event_for_measure(1);
    int evEnd = m_pTable->get_last_event();

    play_segment(evStart, evEnd);
}

//---------------------------------------------------------------------------------------
void ScorePlayer::play_measure(int nMeasure, bool fVisualTracking,
                               long nMM, Interactor* pInteractor)
{
    play_measures(nMeasure, 1, fVisualTracking, nMM, pInteractor);
}

//---------------------------------------------------------------------------------------
void ScorePlayer::play_from_measure(int nMeasure, bool fVisualTracking,
                                    long nMM, Interactor* pInteractor)
{
    // Playback from measure n (n = 1 ... num_measures) to end

    m_fVisualTracking = fVisualTracking;
    m_nMM = nMM;
    m_pInteractor = pInteractor;

    //remember:
    //   real measures 1..n correspond to table items 1..n
    //   items 0 and n+1 are fictitius measures for pre and post control events
    int nEvStart = m_pTable->get_first_event_for_measure(nMeasure);
    int numMeasures = m_pTable->get_num_measures();
    while (nEvStart == -1 && nMeasure < numMeasures)
    {
        //Current measure is empty. Start in next one
        nEvStart = m_pTable->get_first_event_for_measure(++nMeasure);
    }

    if (nEvStart == -1)
        return;     //all measures are empty after selected one!

    int nEvEnd = m_pTable->get_last_event();

    play_segment(nEvStart, nEvEnd);
}

//---------------------------------------------------------------------------------------
void ScorePlayer::play_measures(int startMeasure, int numMeasures, bool fVisualTracking,
                                long nMM, Interactor* pInteractor)
{
    // Playback numMeasures starting in measure startMeasure (1 ... num_measures)

    m_fVisualTracking = fVisualTracking;
    m_nMM = nMM;
    m_pInteractor = pInteractor;

    //remember:
    //   real measures 1..n correspond to table items 1..n
    //   items 0 and n+1 are fictitius measures for pre and post control events
    int evStart = m_pTable->get_first_event_for_measure(startMeasure);
    int maxMeasure = m_pTable->get_num_measures();
    while (evStart == -1 && startMeasure < maxMeasure)
    {
        //Current measure is empty. Start in next one
        evStart = m_pTable->get_first_event_for_measure(++startMeasure);
    }

    if (evStart == -1)
        return;     //all measures are empty after selected one!

    int lastMeasure = min(startMeasure + numMeasures, maxMeasure+1);
    int evEnd;
    if (lastMeasure > maxMeasure)
        evEnd = m_pTable->get_last_event();
    else
        evEnd = m_pTable->get_first_event_for_measure(lastMeasure) - 1;

    play_segment(evStart, evEnd);
}

//---------------------------------------------------------------------------------------
void ScorePlayer::play_segment(int nEvStart, int nEvEnd)
{
    LOMSE_LOG_DEBUG(Logger::k_score_player, ">>[ScorePlayer::play_segment]");
    m_fQuit = false;
    m_fFinalEventSent = false;

    //Create a new thread. It starts inmediately to execute do_play()
    delete m_pThread;
    m_fPlaying = true;
    m_pThread = LOMSE_NEW SoundThread(&ScorePlayer::thread_main, this,
                                nEvStart, nEvEnd, m_fVisualTracking,
                                m_nMM, m_pInteractor);
    LOMSE_LOG_DEBUG(Logger::k_score_player, "<<[ScorePlayer::play_segment]");
}

//---------------------------------------------------------------------------------------
void ScorePlayer::thread_main(int nEvStart, int nEvEnd, bool fVisualTracking,
                              long nMM, Interactor* pInteractor)
{
    LOMSE_LOG_DEBUG(Logger::k_score_player, ">>[ScorePlayer::thread_main]");
    try
    {
        if (pInteractor && !m_fPostEvents)
            pInteractor->enable_forced_view_updates(false);
        fVisualTracking &= (pInteractor != nullptr);
        do_play(nEvStart, nEvEnd, fVisualTracking, nMM, pInteractor);
    }
    catch (boost::thread_interrupted&)
    {
        LOMSE_LOG_DEBUG(Logger::k_score_player, "  [ScorePlayer::thread_main] catching interrupt");
    }

    end_of_playback_housekeeping(fVisualTracking, pInteractor);
    m_fPlaying = false;

    LOMSE_LOG_DEBUG(Logger::k_score_player, "<<[ScorePlayer::thread_main]");
}

//---------------------------------------------------------------------------------------
void ScorePlayer::quit()
{
    //when the user application quits, it is necessary to stop the player without
    //generating repaint or other events. That's the purpose of this method.

    m_fQuit = true;
    stop();
}

//---------------------------------------------------------------------------------------
void ScorePlayer::pause()
{
    if (!m_pThread) return;

    m_fPaused = !m_fPaused;

    if (m_fPaused)
        m_pMidi->all_sounds_off();
    else
        m_canPlay.notify_one();
}

//---------------------------------------------------------------------------------------
void ScorePlayer::stop()
{
    LOMSE_LOG_DEBUG(Logger::k_score_player, ">> Enter");
    if (m_pThread)
    {
        m_fShouldStop = true;

        //wait 500 ms for termination
        if(!m_pThread->timed_join(boost::posix_time::milliseconds(500)))
        {
            LOMSE_LOG_DEBUG(Logger::k_score_player, "Not finished in 500ms. Force interrupt");
            m_pThread->interrupt();
            if(!m_pThread->timed_join(boost::posix_time::seconds(2)))
            {
                LOMSE_LOG_ERROR("Interrupt failed. Force terminate");
                //throw runtime_error("[ScorePlayer::stop] Thread interrupt failed");
            }
        }

        m_pTable->reset_jumps();

        LOMSE_LOG_DEBUG(Logger::k_score_player, "Delete thread");
        delete m_pThread;
        m_pThread = nullptr;
        m_fShouldStop = false;
    }
    LOMSE_LOG_DEBUG(Logger::k_score_player, "<< Exit");
}

//---------------------------------------------------------------------------------------
// Methods to be executed in the thread
//---------------------------------------------------------------------------------------

void ScorePlayer::do_play(int nEvStart, int nEvEnd, bool fVisualTracking,
                          long nMM, Interactor* pInteractor)
{
    // This is the real method doing the work. It is executed inside a
    // different thread.

    LOMSE_LOG_DEBUG(Logger::k_score_player, ">> Enter");
    // if no MIDI server or not inside a thread, return
    if (!m_pMidi || !m_pThread)
    {
        LOMSE_LOG_DEBUG(Logger::k_score_player, "<< Enter. No Midi or no thread. << Exit");
        return;
    }

    //TODO All issues related to sol-fa voice playback

    std::vector<SoundEvent*>& events = m_pTable->get_events();
    if (events.size() == 0)
    {
        LOMSE_LOG_DEBUG(Logger::k_score_player, "<< Enter. No events to play. << Exit");
        return;
    }

    const long k_QUARTER_DURATION = 64L;    //duration (LDP units) of a quarter note (to convert to milliseconds)
    const int k_SOLFA_NOTE = 60;            //pitch for sight reading with percussion sound
    int nPercussionChannel = m_MtrChannel;        //channel to use for percussion

    //options
    bool fCountOff = m_pPlayerGui->countoff_status();
    int playMode = m_pPlayerGui->get_play_mode();
    bool fPlayWithMetronome = m_pPlayerGui->metronome_status();

    //mute general metronome
    if (m_pMtr)
        m_pMtr->mute(true);

    //Prepare instrument for metronome. Instruments for music voices
    //are prepared by events of type ProgInstr
    m_pMidi->program_change(m_MtrChannel, m_MtrInstr);

    //-----------------------------------------------------------------------------------
    //Naming convention for variables:
    //  DeltaTime:  content is LenMus Time Units (TU). One quarte note = 64TU.
    //  Time: content is absolute time (milliseconds)
    //-----------------------------------------------------------------------------------

    //declaration of some time related variables.
    long nEvTime;           //time for next event
    long nMtrEvDeltaTime;   //time for next metronome click

    //default beat and metronome information. It is going to be properly set
    //when a SoundEvent::k_RhythmChange event is found (a time signature object). So these
    //default settings will be used when no time signature in the score.
    m_nMtrPulseDuration = k_QUARTER_DURATION;                     //a beat duration, in TU
    long nMtrIntvalOff = min(7L, m_nMtrPulseDuration / 4L);            //click sound duration, in TU
    long nMtrIntvalNextClick = m_nMtrPulseDuration - nMtrIntvalOff;    //interval from click off to next click
    long nMeasureDuration = m_nMtrPulseDuration * 4;                   //in TU. Assume 4/4 time signature
    long nMtrNumPulses = 4;                                          //assume 4/4 time signature

    //boost::this_thread::disable_interruption di;
    //from this point, interruptions disabled -------------------------------------------

    //Execute control events that take place before the segment to play, so that
    //instruments and tempo are properly programmed. Continue in the loop while
    //we find control events in segment to play.
    int i = 0;
    bool fContinue = true;
    while (fContinue)
    {
        if (events[i]->EventType == SoundEvent::k_prog_instr)
        {
            //change program
            switch (playMode)
            {
                case k_play_rhythm_instrument:
                    m_pMidi->voice_change(events[i]->Channel, 57);        //57 = Trumpet
                    break;
                case k_play_rhythm_percussion:
                    m_pMidi->voice_change(events[i]->Channel, 66);        //66 = High Timbale
                    break;
                case k_play_rhythm_human_voice:
                    //do nothing. Wave sound will be used
                    break;
                case k_play_normal_instrument:
                default:
                    m_pMidi->voice_change(events[i]->Channel, events[i]->Instrument);
            }
        }
        else if (events[i]->EventType == SoundEvent::k_rhythm_change)
        {
            //set up new beat and metronome information
            nMeasureDuration = events[i]->MeasureDuration;
            nMtrNumPulses = events[i]->NumPulses;
            m_nMtrPulseDuration = nMeasureDuration / nMtrNumPulses;       //a pulse duration, in TU
            nMtrIntvalOff = min(7L, m_nMtrPulseDuration / 4L);            //click sound duration (interval to click off), in TU
            nMtrIntvalNextClick = m_nMtrPulseDuration - nMtrIntvalOff;    //interval from click off to next click, in TU
        }
        else
        {
            // it is not a control event. Continue in the loop only
            // if we have not reached the start of the segment to play
            fContinue = (i < nEvStart);
        }
        if (fContinue) i++;
    }
    //Here i points to the first event of desired measure that is not a control event,
    //that is, to first event to play

    //Define and initialize time counter. If playback starts not at the begining but
	//in another measure, advance time counter to that measure
    long curTime = 0L;
	if (nEvStart > 1)
		curTime = delta_to_milliseconds( events[nEvStart]->DeltaTime );

    // get metronome interval duration, in milliseconds
    //Tempo speed for all play methods is controlled by the metronome (PlayerGui) that
    //was specified in method load_score(). Nevertheless, metronome speed can be
    //overriden to force a predefined speed by specifying a non-zero value for
    //parameter nMM.
    if (nMM == 0)
        m_nMtrClickIntval = 60000L / m_pPlayerGui->get_metronome_mm();
    else
        m_nMtrClickIntval = (nMM == 0 ? 1000L : 60000L/nMM);

    //determine last metronome pulse before first note to play.
    //First note could be syncopated or an off-beat note. Round time to nearest
    //lower pulse time
    nMtrEvDeltaTime = ((events[i]->DeltaTime / m_nMtrPulseDuration) - 1) * m_nMtrPulseDuration;
    curTime = delta_to_milliseconds( nMtrEvDeltaTime );

    //prepare weak_ptr to interactor
    WpInteractor wpInteractor;
    if (pInteractor)
    {
        SpInteractor sp = pInteractor->get_shared_ptr_from_this();
        wpInteractor = WpInteractor(sp);
    }
    else
        wpInteractor = WpInteractor();

    //define and prepare highlight event
    SpEventScoreHighlight pEvent(
            LOMSE_NEW EventScoreHighlight(wpInteractor, m_pScore->get_id()) );

    bool fFirstBeatInMeasure = true;    //first beat of a measure
    bool fCountOffPulseActive = false;

    //generate count off metronome clicks. Number of pulses will be the necessary
    //pulses before first anacrusis note, or full measure if no anacrusis.
    //At least two pulses.
    bool fMtrOn = false;                //if true, next metronome event is start
    if (fCountOff)
    {
        //determine num pulses
        int numPulses = 0;
        TimeUnits prevTime = m_pTable->get_anacrusis_missing_time();
        if (is_greater_time(prevTime, 0.0))
        {
            numPulses = int(prevTime + 0.5) / m_nMtrPulseDuration;

            //if anacrusis and first event is a rest (real or implicit), add
            //one additional pulse
            bool fAddExtraPulse = false;

            //check for implicit rest
            if (numPulses * m_nMtrPulseDuration < events[i]->DeltaTime)
                fAddExtraPulse = true;  //implicit rest

            //check for real rest
            else
            {
                fAddExtraPulse = true;      //assume real rest
                long time = events[i]->DeltaTime;
                while (events[i]->DeltaTime == time)
                {
                    if (events[i]->pSO && events[i]->pSO->is_note())
                    {
                        fAddExtraPulse = false;
                        break;
                    }
                    ++i;
                }
            }

            if (fAddExtraPulse)
            {
                ++numPulses;
                nMtrEvDeltaTime += m_nMtrPulseDuration;
                curTime = delta_to_milliseconds( nMtrEvDeltaTime );
            }
        }

        //force two pulses at least
        if (numPulses < 2)
            numPulses += nMtrNumPulses;

        //generate the pulses
        for (int j=numPulses; j > 1; --j)
        {
            //generate click
            m_pMidi->note_on(m_MtrChannel, m_MtrTone2, 127);
            boost::posix_time::milliseconds waitTime(m_nMtrClickIntval/2L);
            boost::this_thread::sleep(waitTime);
            m_pMidi->note_off(m_MtrChannel, m_MtrTone2, 127);
            boost::this_thread::sleep(waitTime);
        }
        //generate final click
        m_pMidi->note_on(m_MtrChannel, m_MtrTone1, 127);
        if (fVisualTracking)
        {
            if (events[i]->pSO)
                pEvent->add_item(EventScoreHighlight::k_advance_tempo_line, events[i]->pSO->get_id());
            if (nMtrEvDeltaTime < events[i]->DeltaTime)
            {
                //last metronome click is previous to first event from table.
                //send highlight event
                if (m_fPostEvents)
                    m_libScope.post_event(pEvent);
                else if (pInteractor)
                    pInteractor->handle_event(pEvent);
                pEvent = SpEventScoreHighlight(
                            LOMSE_NEW EventScoreHighlight(wpInteractor,
                                                          m_pScore->get_id()) );
            }
        }

        fMtrOn = true;
        nMtrEvDeltaTime += nMtrIntvalOff;
        fFirstBeatInMeasure = false;
        fCountOffPulseActive = true;
    }

    //loop to process events
    do
    {
        //Verify if next event is a metronome click
        if (nMtrEvDeltaTime <= events[i]->DeltaTime)
        {
            //Next event should be a metronome click or the click off event for the previous metronome click
            nEvTime = delta_to_milliseconds(nMtrEvDeltaTime);
            if (curTime < nEvTime)
            {
                //flush pending events
                long elapsed = 0L;
                if (fVisualTracking && pEvent->get_num_items() > 0)
                {
                    clock_t t1=clock();
                    if (m_fPostEvents)
                        m_libScope.post_event(pEvent);
                    else if (pInteractor)
                        pInteractor->handle_event(pEvent);
                    pEvent = SpEventScoreHighlight(
                                LOMSE_NEW EventScoreHighlight(wpInteractor,
                                                              m_pScore->get_id()) );
                    clock_t t2=clock();
                    elapsed = long( ((t2-t1)*1000.0)/double(CLOCKS_PER_SEC));
                }

                //wait for current time
                long waitT = nEvTime - curTime - elapsed;
                if (waitT > 0L)
                {
                    boost::posix_time::milliseconds waitTime(waitT);
                    boost::this_thread::sleep(waitTime);
                }
                curTime = nEvTime;
            }

            if (fMtrOn)
            {
                //the event is the click off for the previous metronome click
                if (fPlayWithMetronome || fCountOffPulseActive)
                {
                    if (fFirstBeatInMeasure)
                        m_pMidi->note_off(m_MtrChannel, m_MtrTone1, 127);
                    else
                        m_pMidi->note_off(m_MtrChannel, m_MtrTone2, 127);

                    fCountOffPulseActive = false;
                }
                fMtrOn = false;
                nMtrEvDeltaTime += nMtrIntvalNextClick;
            }
            else
            {
                //the event is a metronome click
                fFirstBeatInMeasure = (nMtrEvDeltaTime % nMeasureDuration == 0);
                if (fPlayWithMetronome)
                {
                    if (fFirstBeatInMeasure)
                        m_pMidi->note_on(m_MtrChannel, m_MtrTone1, 127);
                    else
                        m_pMidi->note_on(m_MtrChannel, m_MtrTone2, 127);
                }
                if (fVisualTracking && events[i]->pSO != nullptr)
                    pEvent->add_item(EventScoreHighlight::k_advance_tempo_line, events[i]->pSO->get_id());

                fMtrOn = true;
                nMtrEvDeltaTime += nMtrIntvalOff;
            }
            curTime = nEvTime;
        }
        else
        {
            //next even comes from the table. Usually it will be a note on/off
            nEvTime = delta_to_milliseconds( events[i]->DeltaTime );
            if (nEvTime > curTime)
            {
                //flush accumulated events for curTime
                long elapsed = 0L;
                if (fVisualTracking && pEvent->get_num_items() > 0)
                {
                    clock_t t1=clock();
                    if (m_fPostEvents)
                        m_libScope.post_event(pEvent);
                    else if (pInteractor)
                        pInteractor->handle_event(pEvent);
                    pEvent = SpEventScoreHighlight(
                                LOMSE_NEW EventScoreHighlight(wpInteractor,
                                                              m_pScore->get_id()) );
                    clock_t t2=clock();
                    elapsed = long( ((t2-t1)*1000.0)/double(CLOCKS_PER_SEC));
                }

                //wait until new time arrives
                long waitT = nEvTime - curTime - elapsed;
                if (waitT > 0L)
                {
                    boost::posix_time::milliseconds waitTime(waitT);
                    boost::this_thread::sleep(waitTime);
                }
            }

            //if it is a jump event, execute the jump if applicable
            if (events[i]->EventType == SoundEvent::k_jump)
            {
                bool fExecuted = false;
                JumpEntry* pJump = events[i]->pJump;
                if (pJump->get_visited() >= pJump->get_times_before())
                {
                    if (pJump->get_times_valid() == 0
                        || pJump->get_times_valid() > pJump->get_executed())
                    {
                        i = pJump->get_event();
                        nEvTime = delta_to_milliseconds( events[i]->DeltaTime );
                        curTime = nEvTime;
                        nMtrEvDeltaTime = events[i]->DeltaTime;
                        if (pJump->get_times_valid() > pJump->get_executed())
                            pJump->increment_applied();
                        fExecuted = true;
                    }
                }

                pJump->increment_visited();

                if (!fExecuted)
                    ++i;

                continue;   //needed if next event is also a jump, and for metronome clicks
            }


            if (events[i]->EventType == SoundEvent::k_note_on)
            {
                //start of note
                switch(playMode)
                {
                    case k_play_rhythm_instrument:
                        m_pMidi->note_on(events[i]->Channel, k_SOLFA_NOTE,
                                        events[i]->Volume);
                        break;
                    case k_play_rhythm_percussion:
                        m_pMidi->note_on(nPercussionChannel, k_SOLFA_NOTE,
                                        events[i]->Volume);
                        break;
                    case k_play_rhythm_human_voice:
                        //WaveOn .NoteStep, events[i]->Volume);
                        break;
                    case k_play_normal_instrument:
                    default:
                        m_pMidi->note_on(events[i]->Channel, events[i]->NotePitch,
                                        events[i]->Volume);
                }

                //generate implicit visual on event
                if (fVisualTracking && events[i]->pSO->is_visible())
                {
                    ImoId id = events[i]->pSO->get_id();
                    m_pInteractor->change_viewport_if_necessary(id);
                    pEvent->add_item(EventScoreHighlight::k_highlight_on, id);
                }

            }
            else if (events[i]->EventType == SoundEvent::k_note_off)
            {
                //end of note
                switch(playMode)
                {
                    case k_play_rhythm_instrument:
                        m_pMidi->note_off(events[i]->Channel, k_SOLFA_NOTE, 127);
                        break;
                    case k_play_rhythm_percussion:
                        m_pMidi->note_off(nPercussionChannel, k_SOLFA_NOTE, 127);
                        break;
                    case k_play_rhythm_human_voice:
                        //WaveOff
                        break;
                    case k_play_normal_instrument:
                    default:
                        m_pMidi->note_off(events[i]->Channel, events[i]->NotePitch, 127);
                }

                //generate implicit visual off event
                if (fVisualTracking && events[i]->pSO->is_visible())
                    pEvent->add_item(EventScoreHighlight::k_highlight_off, events[i]->pSO->get_id());
            }
            else if (events[i]->EventType == SoundEvent::k_visual_on)
            {
                //set visual highlight
                if (fVisualTracking)
                {
                    ImoId id = events[i]->pSO->get_id();
                    m_pInteractor->change_viewport_if_necessary(id);
                    pEvent->add_item(EventScoreHighlight::k_highlight_on, id);
                }
            }
            else if (events[i]->EventType == SoundEvent::k_visual_off)
            {
                //remove visual highlight
                if (fVisualTracking)
                    pEvent->add_item(EventScoreHighlight::k_highlight_off, events[i]->pSO->get_id());

            }
            else if (events[i]->EventType == SoundEvent::k_end_of_score)
            {
                //end of table
                break;
            }
            else if (events[i]->EventType == SoundEvent::k_rhythm_change)
            {
                //set up new beat and metronome information
                nMeasureDuration = events[i]->MeasureDuration;
                nMtrNumPulses = events[i]->NumPulses;
                m_nMtrPulseDuration = nMeasureDuration / nMtrNumPulses;        //a pulse duration
                nMtrIntvalOff = min(7L, m_nMtrPulseDuration / 4L);            //click duration (interval to click off)
                nMtrIntvalNextClick = m_nMtrPulseDuration - nMtrIntvalOff;    //interval from click off to next click
            }
            else if (events[i]->EventType == SoundEvent::k_prog_instr)
            {
                //change program
                switch (playMode)
                {
                    case k_play_rhythm_instrument:
                        m_pMidi->voice_change(events[i]->Channel, 57);        //57 = Trumpet
                        break;
                    case k_play_rhythm_percussion:
                        m_pMidi->voice_change(events[i]->Channel, 66);        //66 = High Timbale
                        break;
                    case k_play_rhythm_human_voice:
                        //do nothing. Wave sound will be used
                        break;
                    case k_play_normal_instrument:
                    default:
                        m_pMidi->voice_change(events[i]->Channel, events[i]->NotePitch);
                }
            }
            else
            {
                //program error. Unknown event type. Ignore
                //TODO: log event
            }

            curTime = max(curTime, nEvTime);        //to avoid going backwards when no metronome
                                                //before start and progInstr events
            i++;
        }

        //check if the thread should be paused or stopped
        if (m_fShouldStop)
        {
            LOMSE_LOG_DEBUG(Logger::k_score_player, "Going to finish 1");
            break;
        }
        while(m_fPaused)
        {
            boost::this_thread::sleep( boost::posix_time::milliseconds(200) );
            if (m_fShouldStop)
            {
                LOMSE_LOG_DEBUG(Logger::k_score_player, "Going to finish 2");
                break;
            }
        }

        //update metronome information, just in case metronome was updated
        if (nMM == 0)   //AWARE: nMM==0 means: "read tempo from GUI controls"
        {
            long newMtrClickIntval = 60000L / m_pPlayerGui->get_metronome_mm();
            if (newMtrClickIntval != m_nMtrClickIntval)
            {
                m_nMtrClickIntval = newMtrClickIntval;
                curTime = delta_to_milliseconds( events[i-1]->DeltaTime );
            }
        }
        fPlayWithMetronome = m_pPlayerGui->metronome_status();

    } while (i <= nEvEnd);

    //ensure that all visual highlight is removed
    if (fVisualTracking && !m_fQuit)
    {
        m_fFinalEventSent = true;
        SpEventScoreHighlight pEvent(
            LOMSE_NEW EventScoreHighlight(wpInteractor, m_pScore->get_id()) );
        pEvent->add_item(EventScoreHighlight::k_end_of_higlight, k_no_imoid);
        if (m_fPostEvents)
            m_libScope.post_event(pEvent);
        else if (pInteractor)
            pInteractor->handle_event(pEvent);
    }
    LOMSE_LOG_DEBUG(Logger::k_score_player, "<< Exit");
}

//---------------------------------------------------------------------------------------
void ScorePlayer::end_of_playback_housekeeping(bool fVisualTracking,
                                               Interactor* pInteractor)
{
    LOMSE_LOG_DEBUG(Logger::k_score_player, "<< Enter");

    //ensure that all visual highlight is removed
    if (fVisualTracking && !m_fQuit && !m_fFinalEventSent)
    {
        m_fFinalEventSent = true;
        WpInteractor wpInteractor;
        if (pInteractor)
        {
            SpInteractor sp = pInteractor->get_shared_ptr_from_this();
            wpInteractor = WpInteractor(sp);
        }
        else
            wpInteractor = WpInteractor();
        SpEventScoreHighlight pEvent(
            LOMSE_NEW EventScoreHighlight(wpInteractor, m_pScore->get_id()) );
        pEvent->add_item(EventScoreHighlight::k_end_of_higlight, k_no_imoid);
        if (m_fPostEvents)
            m_libScope.post_event(pEvent);
        else if (pInteractor)
            pInteractor->handle_event(pEvent);
    }

    //ensure that all sounds are off
    m_pMidi->all_sounds_off();

    //reset all jumps
    m_pTable->reset_jumps();

    //do not generate events if quit
    if (m_fQuit)
    {
        LOMSE_LOG_DEBUG(Logger::k_score_player, "<< Exit");
        return;
    }

    //enable again the general metronome
    if (m_pMtr)
        m_pMtr->mute(false);

    // allow view updates
    if (pInteractor)
        pInteractor->enable_forced_view_updates(true);

    //create event for updating player gui
    if (m_pPlayerGui)
    {
        //prepare weak_ptr to interactor
        WpInteractor wpInteractor;
        if (pInteractor)
        {
            SpInteractor sp = pInteractor->get_shared_ptr_from_this();
            wpInteractor = WpInteractor(sp);
        }
        else
            wpInteractor = WpInteractor();

        SpEventEndOfPlayback event(
            LOMSE_NEW EventEndOfPlayback(k_end_of_playback_event, wpInteractor,
                                         m_pScore, m_pPlayerGui) );
        if (m_fPostEvents)
            m_libScope.post_event(event);
        else if (pInteractor)
            pInteractor->handle_event(event);
    }
    LOMSE_LOG_DEBUG(Logger::k_score_player, "<< Exit");
}


}   //namespace lomse

