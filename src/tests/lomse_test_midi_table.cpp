//---------------------------------------------------------------------------------------
// This file is part of the Lomse library.
// Lomse is copyrighted work (c) 2010-2017. All rights reserved.
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

#include <UnitTest++.h>
#include <sstream>
#include "lomse_build_options.h"

//classes related to these tests
#include "lomse_injectors.h"
#include "lomse_midi_table.h"
#include "lomse_document.h"
#include "lomse_internal_model.h"
//#include "lomse_staffobjs_table.h"


using namespace UnitTest;
using namespace std;
using namespace lomse;

//---------------------------------------------------------------------------------------
//Helper, to access protected members
class MySoundEventsTable : public SoundEventsTable
{
public:
    MySoundEventsTable(ImoScore* pScore) : SoundEventsTable(pScore) {}
    virtual ~MySoundEventsTable() {}

    void my_program_sounds_for_instruments() { program_sounds_for_instruments(); }
    void my_create_events() { create_events(); }
    void my_close_table() { close_table(); }
    void my_sort_by_time() { sort_by_time(); }

};

//---------------------------------------------------------------------------------------
class MidiTableTestFixture
{
public:
    LibraryScope m_libraryScope;
    string m_scores_path;
    Document* m_pDoc;
    ImoScore* m_pScore;
    SoundEventsTable* m_pTable;

    MidiTableTestFixture()     //SetUp fixture
        : m_libraryScope(cout)
        , m_scores_path(TESTLIB_SCORES_PATH)
        , m_pDoc(nullptr)
    {
        m_scores_path = TESTLIB_SCORES_PATH;
        m_scores_path += "unit-tests/repeats/";
        m_libraryScope.set_default_fonts_path(TESTLIB_FONTS_PATH);
    }

    ~MidiTableTestFixture()    //TearDown fixture
    {
        delete m_pDoc;
        m_pDoc = nullptr;
    }

    inline const char* test_name()
    {
        return UnitTest::CurrentTest::Details()->testName;
    }

    void load_mxl_score_for_test(const std::string& score)
    {
        string filename = m_scores_path + score;
        ifstream score_file(filename.c_str());
        if (!score_file.good())
            cout  << test_name() << ". Unit test filename not found: " << filename << endl;

        m_pDoc = LOMSE_NEW Document(m_libraryScope, cout);
        m_pDoc->from_file(filename, Document::k_format_mxl);
        m_pScore = static_cast<ImoScore*>( m_pDoc->get_im_root()->get_content_item(0) );

        m_pTable = m_pScore->get_midi_table();
    }

    void load_ldp_score_for_test(const std::string& score)
    {
        string filename = m_scores_path + score;
        ifstream score_file(filename.c_str());
        if (!score_file.good())
            cout  << test_name() << ". Unit test filename not found: " << filename << endl;

        m_pDoc = LOMSE_NEW Document(m_libraryScope, cout);
        m_pDoc->from_file(filename, Document::k_format_ldp);
        m_pScore = static_cast<ImoScore*>( m_pDoc->get_im_root()->get_content_item(0) );

        m_pTable = m_pScore->get_midi_table();
    }

    bool check_jump(int i, int measure, int timesValid, int timesBefore, int event)
    {
        JumpEntry* pEntry = static_cast<JumpEntry*>( m_pTable->get_jump(i) );
        if (pEntry->get_measure() != measure
            || pEntry->get_times_valid() != timesValid
            || pEntry->get_times_before() != timesBefore
            || pEntry->get_executed() != 0
            || pEntry->get_visited() != 0
            || pEntry->get_event() != event )
        {
            cout << test_name() << ". JumpEntry " << i << ": "
                 << pEntry->dump_entry();
            return false;
        }
        return true;
    }

    bool check_jump(int i, int measure, int timesValid, int timesBefore=0)
    {
        JumpEntry* pEntry = static_cast<JumpEntry*>( m_pTable->get_jump(i) );
        if (pEntry->get_measure() != measure
            || pEntry->get_times_valid() != timesValid
            || pEntry->get_times_before() != timesBefore
            || pEntry->get_executed() != 0
            || pEntry->get_visited() != 0 )
        {
            cout << test_name() << ". JumpEntry " << i << ": "
                 << pEntry->dump_entry();
            return false;
        }
        return true;
    }

};

SUITE(MidiTableTest)
{

    TEST_FIXTURE(MidiTableTestFixture, ProgramSounds)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(n c4 q) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();

        //cout << "num.events = " << table.num_events() << endl;
        CHECK( table.num_events() == 1 );
        std::vector<SoundEvent*>& events = table.get_events();
        SoundEvent* ev = events.front();
        CHECK( ev->Channel == 0 );
        CHECK( ev->Instrument == 0 );
        CHECK( ev->EventType == SoundEvent::k_prog_instr );
    }

    TEST_FIXTURE(MidiTableTestFixture, ProgramSoundsMidiInfo)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (infoMIDI 2 0)(musicData (clef G)(n c4 q) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();

        //cout << "num.events = " << table.num_events() << endl;
        CHECK( table.num_events() == 1 );
        std::vector<SoundEvent*>& events = table.get_events();
        SoundEvent* ev = events.front();
        CHECK( ev->Channel == 0 );
        CHECK( ev->Instrument == 2 );
        CHECK( ev->EventType == SoundEvent::k_prog_instr );
    }

    TEST_FIXTURE(MidiTableTestFixture, CreateEvents_OneNote)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(n c4 q) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();

        CHECK( table.num_events() == 3 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_on );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_off );
    }

    TEST_FIXTURE(MidiTableTestFixture, CreateEvents_OneRest)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(r q) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();

        //cout << "num.events = " << table.num_events() << endl;
        CHECK( table.num_events() == 3 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_visual_on );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_visual_off );
    }

    TEST_FIXTURE(MidiTableTestFixture, CreateEvents_RestNoVisible)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(r q noVisible)(n c4 q) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();

        //cout << "num.events = " << table.num_events() << endl;
        CHECK( table.num_events() == 3 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_on );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_off );
    }

    TEST_FIXTURE(MidiTableTestFixture, CreateEvents_TwoNotesTied)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(n c4 q l)(n c4 e) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();

        CHECK( table.num_events() == 5 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_on );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_visual_off );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_visual_on );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_off );
    }

    TEST_FIXTURE(MidiTableTestFixture, BarlineIncrementsMeasureCount)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(n c4 q)(barline)(n c4 e) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();

        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_on );
        CHECK( (*it)->Measure == 1 );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_off );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_on );
        CHECK( (*it)->Measure == 2 );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_off );
    }

    TEST_FIXTURE(MidiTableTestFixture, TimeSignatureAddsRythmChange)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(time 2 4) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();

        CHECK( table.num_events() == 2 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_rhythm_change );
        CHECK( (*it)->NumPulses == 2 );
        CHECK( (*it)->MeasureDuration == 128 );
        //cout << ", NumPulses = " << (*it)->NumPulses
        //     << ", MeasureDuration = " << (*it)->MeasureDuration << endl;
    }

    TEST_FIXTURE(MidiTableTestFixture, TimeSignatureInfoOk)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(time 6 8) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();

        CHECK( table.num_events() == 2 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_rhythm_change );
        CHECK( (*it)->NumPulses == 2 );
        CHECK( (*it)->MeasureDuration == 192 );
        //cout << ", NumPulses = " << (*it)->NumPulses
        //     << ", MeasureDuration = " << (*it)->MeasureDuration << endl;
    }

    TEST_FIXTURE(MidiTableTestFixture, CloseTableAddsEvent)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (r q) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_close_table();

        CHECK( table.num_events() == 1 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_end_of_score );
        CHECK( (*it)->DeltaTime == 0.0f );
    }

    TEST_FIXTURE(MidiTableTestFixture, CloseTableFinalTime)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (r q) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();
        table.my_close_table();

        CHECK( table.num_events() == 4 );
        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_visual_on );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_visual_off );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_end_of_score );
        CHECK( (*it)->DeltaTime == 64.0f );
    }

    TEST_FIXTURE(MidiTableTestFixture, EventsSorted)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(chord (n c4 q)(n e4 q)) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.my_program_sounds_for_instruments();
        table.my_create_events();
        table.my_close_table();
        table.my_sort_by_time();

        std::vector<SoundEvent*>& events = table.get_events();
        std::vector<SoundEvent*>::iterator it = events.begin();
        CHECK( (*it)->EventType == SoundEvent::k_prog_instr );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_on );
        CHECK( (*it)->DeltaTime == 0.0f );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_on );
        CHECK( (*it)->DeltaTime == 0.0f );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_off );
        CHECK( (*it)->DeltaTime == 64.0f );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_note_off );
        CHECK( (*it)->DeltaTime == 64.0f );
        ++it;
        CHECK( (*it)->EventType == SoundEvent::k_end_of_score );
        CHECK( (*it)->DeltaTime == 64.0f );
    }


    //@ Measures table ------------------------------------------------------------------

    TEST_FIXTURE(MidiTableTestFixture, MeasuresTable)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(chord (n c4 q)(n e4 q)) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.create_table();

        std::vector<SoundEvent*>& events = table.get_events();

        int iEv = table.get_first_event_for_measure(1);
        CHECK( iEv == 1 );
        CHECK( events[iEv]->EventType == SoundEvent::k_note_on );

        iEv = table.get_last_event();
        CHECK( iEv == 5 );
        CHECK( events[iEv]->EventType == SoundEvent::k_end_of_score );

        CHECK( table.get_num_measures() == 1 );
    }

    TEST_FIXTURE(MidiTableTestFixture, MeasuresTable_ExtraFinalMeasure)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(chord (n c4 q)(n e4 q)) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.create_table();

        std::vector<SoundEvent*>& events = table.get_events();

        int iEv = table.get_first_event_for_measure(2);
        CHECK( iEv == 5 );
        CHECK( events[iEv]->EventType == SoundEvent::k_end_of_score );
    }

    TEST_FIXTURE(MidiTableTestFixture, MeasuresTable_InitialControlMeasure)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(chord (n c4 q)(n e4 q)) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.create_table();

        std::vector<SoundEvent*>& events = table.get_events();

        int iEv = table.get_first_event_for_measure(0);
        CHECK( iEv == 0 );
        CHECK( events[iEv]->EventType == SoundEvent::k_prog_instr );
    }

    TEST_FIXTURE(MidiTableTestFixture, MeasuresTable_TwoMeasures)
    {
        Document doc(m_libraryScope);
        doc.from_string("(lenmusdoc (vers 0.0) (content (score (vers 1.6) "
            "(instrument (musicData (clef G)(n c4 q)(barline)(n c4 e) )) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );
        MySoundEventsTable table(pScore);
        table.create_table();

        std::vector<SoundEvent*>& events = table.get_events();

        int iEv = table.get_first_event_for_measure(0);
        CHECK( iEv == 0 );
        CHECK( events[iEv]->EventType == SoundEvent::k_prog_instr );

        iEv = table.get_first_event_for_measure(1);
        CHECK( iEv == 1 );
        CHECK( events[iEv]->EventType == SoundEvent::k_note_on );

        iEv = table.get_first_event_for_measure(2);
        CHECK( iEv == 3 );
        CHECK( events[iEv]->EventType == SoundEvent::k_note_on );

        iEv = table.get_first_event_for_measure(3);
        CHECK( iEv == 5 );
        CHECK( events[iEv]->EventType == SoundEvent::k_end_of_score );

        iEv = table.get_last_event();
        CHECK( iEv == 5 );
        CHECK( events[iEv]->EventType == SoundEvent::k_end_of_score );

        CHECK( table.get_num_measures() == 2 );
    }


    //@ Jumps table ------------------------------------------------------------------

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_01)
    {
        //@001. empty score creates empty table
        Document doc(m_libraryScope);
        doc.from_string("(score (vers 2.0)(instrument (musicData )))");
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );

		SoundEventsTable* pTable = pScore->get_midi_table();

        CHECK( pTable->num_jumps() == 0 );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_02)
    {
        //@002. score with no repetitions creates empty table
        Document doc(m_libraryScope);
        doc.from_string("(score (vers 2.0)(instrument (musicData "
            "(clef G)(n c4 q)(n e4 q)(n g4 q) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );

		SoundEventsTable* pTable = pScore->get_midi_table();

        CHECK( pTable->num_jumps() == 0 );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_03)
    {
        //@003. [T1] end-repetition barline creates one repetition
        //  |    |    |    :|     |     |
        //  1    2    3     4     5
        //                 J 2,1
        load_mxl_score_for_test("01-repeat-end-repetition-barline.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 1 );
        CHECK( check_jump(0, 1,1,0,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_04)
    {
        //@004. [T2] start-end-repetition barlines creates one repetition
        //  |    |     |:    |    :|     |     |
        //  1    2     3     4     5     6
        //                       J 3,1
        load_mxl_score_for_test("02-repeat-start-end-repetition-barlines.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 1 );
        CHECK( check_jump(0, 3,1,0,6) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_05)
    {
        //@005. As 004 but with LDP score
        //  |    |:    |    :|     |     |
        //  1    2     3     4     5
        //                  J2,1
        Document doc(m_libraryScope, cout);
        doc.from_string("(score (vers 2.0)(instrument (musicData "
            "(clef G)(n c4 q)(n e4 q)(barline startRepetition)"
            "(n c4 q)(n e4 q)(barline)"
            "(n c4 q)(n e4 q)(barline endRepetition)"
            "(n c4 q)(n e4 q)(barline)(n c4 q)(n e4 q)(barline) )))" );
        ImoScore* pScore = static_cast<ImoScore*>( doc.get_im_root()->get_content_item(0) );

		m_pTable = pScore->get_midi_table();

        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 1 );
        CHECK( check_jump(0, 2,1,0,5) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_06)
    {
        //@006. [T3] start-end-repetition barlines creates one repetition
        //  |    |    :|:    |    :|     |     |
        //  1    2     3     4     5     6
        //            J 1,1       J 3,1
        load_mxl_score_for_test("03-repeat-double-end-repetition-barlines.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 2 );
        CHECK( check_jump(0, 1,1,0,1) == true );
        CHECK( check_jump(1, 3,1,0,7) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_07)
    {
        //@007. [T4] end-repetition barline with volta brackets
        //                  vt1    vt2
        //  |    |    |     |     :|     |     |     |
        //  1    2    3     4      5    6
        //                 J 4,1  J 1,1
        //                   5,0
        load_mxl_score_for_test("04-repeat-barline-simple-volta.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 3 );
        CHECK( check_jump(0, 4,1,0,10) == true );
        CHECK( check_jump(1, 5,0,0,13) == true );
        CHECK( check_jump(2, 1,1,0,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_08)
    {
        //@008. [T5]  end-repetition barline with volta, two times
        //                  vt1,2  vt3
        //  |    |    |     |     :|     |     |     |
        //  1    2    3     4      5    6
        //                 J4,2   J1,2
        //                 J5,0
        load_mxl_score_for_test("05-repeat-barline-simple-volta-two-times.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 3 );
        CHECK( check_jump(0, 4,2,0,10) == true );
        CHECK( check_jump(1, 5,0,0,13) == true );
        CHECK( check_jump(2, 1,2,0,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_09)
    {
        //@009. [T7] three voltas
        //            vt1   vt2    vt3
        //  |    |    |    :|     :|     |     |
        //  1    2    3     4      5    6
        //           J3,1  J1,1   J1,1
        //           J4,1
        //           J5,0
        load_mxl_score_for_test("07-repeat-barlines-three-volta.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 5 );
        CHECK( check_jump(0, 3,1) == true );
        CHECK( check_jump(1, 4,1) == true );
        CHECK( check_jump(2, 5,0) == true );
        CHECK( check_jump(3, 1,1) == true );
        CHECK( check_jump(4, 1,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_10)
    {
        //@010. [T8] three voltas long
        //            vt1------- vt2------- vt3---------
        //  |    |    |     |    :|     |    :|     |     |    |    |    |    |
        //  1    2    3     4      5    6     7     8     9    10   11   12
        //           J3,1        J1,1        J1,1
        //           J5,1
        //           J7,0
        load_mxl_score_for_test("08-repeat-barlines-three-volta-long.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 5 );
        CHECK( check_jump(0, 3,1) == true );
        CHECK( check_jump(1, 5,1) == true );
        CHECK( check_jump(2, 7,0) == true );
        CHECK( check_jump(3, 1,1) == true );
        CHECK( check_jump(4, 1,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_11)
    {
        //@011. three voltas long several times
        //            vt1,2------ vt3,5------- vt5---------
        //  |    |    |     |    :|     |    :|     |     |    |    |    |    |
        //  1    2    3     4      5    6     7     8     9    10   11   12
        //           J3,2        J1,2        J1,2
        //           J5,2
        //           J7,0
        load_mxl_score_for_test("09-repeat-barlines-three-volta-long-several-times.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 5 );
        CHECK( check_jump(0, 3,2) == true );
        CHECK( check_jump(1, 5,2) == true );
        CHECK( check_jump(2, 7,0) == true );
        CHECK( check_jump(3, 1,2) == true );
        CHECK( check_jump(4, 1,2) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_51)
    {
        //@051. da capo
        //                            D.C.
        //  |      |      |      |      |
        //  1      2      3      4
        //                             J1,1
        load_mxl_score_for_test("51-repeat-da-capo.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 1 );
        CHECK( check_jump(0, 1,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_52)
    {
        //@052. da capo al fine
        //             Fine           D.C.
        //  |      |      |      |      |
        //  1      2      3      4
        //              J-1,1,1        J1,1
        load_mxl_score_for_test("52-repeat-da-capo-al-fine.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 2);
        CHECK( check_jump(0, -1,1,1) == true );
        CHECK( check_jump(1, 1,1) == true );
    }

//    TEST_FIXTURE(MidiTableTestFixture, jumps_table_53)
//    {
//        //@053. da capo al segno
//        //         Segno               D.C.
//        //  |      |      |      ||      |
//        //  1      2      3       4
//        //        J4,1,1                J1,1
//        load_mxl_score_for_test("53-repeat-da-capo-al-segno.xml");
//        cout << m_pTable->dump_midi_events() << endl;
//        CHECK( m_pTable->num_jumps() == 2);
//        CHECK( check_jump(0, 4,1,1) == true );
//        CHECK( check_jump(1, 1,1) == true );
//    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_54)
    {
        //@054. dal segno al fine
        //         Segno       Fine    D.S.al fine
        //  |      |      |      ||      |
        //  1      2      3       4
        //                     J-1,1,1   J2,1
        load_mxl_score_for_test("54-repeat-dal-segno-al-fine.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 2);
        CHECK( check_jump(0, -1,1,1) == true );
        CHECK( check_jump(1, 2,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_55)
    {
        //@055. dal segno al coda
        //                             To      D.S.al
        //            Segno           Coda      Coda   Coda
        //  |         |         |        |           ||         |
        //  1         2         3        4            5
        //                            J5,1,1        J2,1
        load_mxl_score_for_test("55-repeat-dal-segno-al-coda.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 2);
        CHECK( check_jump(0, 5,1,1) == true );
        CHECK( check_jump(1, 2,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_56)
    {
        //@056. dal segno al coda
        //                                     D.C.al
        //                To Coda                Coda  Coda
        //  |         |         |:      :|           ||         |
        //  1         2         3        4            5
        //                    J5,1,1   J3,1        J1,1
        load_mxl_score_for_test("56-repeat-da-capo-al-coda.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 3);
        CHECK( check_jump(0, 5,1,1) == true );
        CHECK( check_jump(1, 3,1) == true );
        CHECK( check_jump(2, 1,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_57)
    {
        //@057. dal segno
        //            Segno                       D.S.
        //  |         |         |:      :|           ||         |
        //  1         2         3        4            5
        //                             J3,1         J2,1
        load_mxl_score_for_test("57-repeat-dal-segno.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 2);
        CHECK( check_jump(0, 3,1) == true );
        CHECK( check_jump(1, 2,1) == true );
    }

    TEST_FIXTURE(MidiTableTestFixture, jumps_table_58)
    {
        //@058. dal segno al coda
        //                   To                 DS al
        //            Segno  Coda                Coda  Coda
        //  |         |         |:      :|           ||         |
        //  1         2         3        4            5
        //                    J5,1,1   J3,1        J2,1
        load_mxl_score_for_test("58-repeat-dal-segno-al-coda.xml");
        //cout << m_pTable->dump_midi_events() << endl;
        CHECK( m_pTable->num_jumps() == 3);
        CHECK( check_jump(0, 5,1,1) == true );
        CHECK( check_jump(1, 3,1) == true );
        CHECK( check_jump(2, 2,1) == true );
    }

}


