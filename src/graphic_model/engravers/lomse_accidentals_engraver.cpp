//---------------------------------------------------------------------------------------
// This file is part of the Lomse library.
// Copyright (c) 2010-2012 Cecilio Salmeron. All rights reserved.
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

#include "lomse_accidentals_engraver.h"

#include "lomse_engraving_options.h"
#include "lomse_im_note.h"
#include "lomse_glyphs.h"
#include "lomse_shape_note.h"
#include "lomse_shapes.h"
#include "lomse_score_meter.h"


namespace lomse
{

//---------------------------------------------------------------------------------------
// AccidentalsEngraver implementation
//---------------------------------------------------------------------------------------
AccidentalsEngraver::AccidentalsEngraver(LibraryScope& libraryScope, ScoreMeter* pScoreMeter)
    : Engraver(libraryScope, pScoreMeter)
{
}

//---------------------------------------------------------------------------------------
GmoShapeAccidentals* AccidentalsEngraver::create_shape(ImoObj* pCreatorImo,
                                                       int iInstr, int iStaff,
                                                       UPoint uPos,
                                                       EAccidentals accidentals,
                                                       bool fCautionary)
{
    m_accidentals = accidentals;
    m_fCautionary = fCautionary;
    m_iInstr = iInstr;
    m_iStaff = iStaff;
    m_fontSize = determine_font_size();
    m_pCreatorImo = pCreatorImo;

    find_glyphs();
    create_container_shape(pCreatorImo, uPos);
    add_glyphs_to_container_shape(uPos);

    return m_pContainer;
}

//---------------------------------------------------------------------------------------
void AccidentalsEngraver::find_glyphs()
{
    int i = 0;

    if (m_fCautionary)
        m_glyphs[i++] = k_glyph_open_cautionary_accidental;

    switch(m_accidentals)
    {
        case k_natural:
            m_glyphs[i++] = k_glyph_natural_accidental;
            break;
        case k_sharp:
            m_glyphs[i++] = k_glyph_sharp_accidental;
            break;
        case k_flat:
            m_glyphs[i++] = k_glyph_flat_accidental;
            break;
        case k_flat_flat:
            m_glyphs[i++] = k_glyph_double_flat_accidental;
            break;
        case k_double_sharp:
            m_glyphs[i++] = k_glyph_double_sharp_accidental;
            break;
        case k_natural_flat:
            m_glyphs[i++] = k_glyph_natural_accidental;
            m_glyphs[i++] = k_glyph_flat_accidental;
            break;
        case k_natural_sharp:
            m_glyphs[i++] = k_glyph_natural_accidental;
            m_glyphs[i++] = k_glyph_sharp_accidental;
            break;
        case k_sharp_sharp:
            m_glyphs[i++] = k_glyph_sharp_accidental;
            m_glyphs[i++] = k_glyph_sharp_accidental;
            break;
        //case lm_eQuarterFlat:
        //    m_glyphs[i++] = k_glyph_natural_accidental;
        //    break;
        //case lm_eQuarterSharp:
        //    m_glyphs[i++] = k_glyph_natural_accidental;
        //    break;
        //case lm_eThreeQuartersFlat:
        //    m_glyphs[i++] = k_glyph_natural_accidental;
        //    break;
        //case lm_eThreeQuartersSharp:
        //    m_glyphs[i++] = k_glyph_natural_accidental;
        //    break;
        default:
            m_glyphs[i++] = k_glyph_natural_accidental;
            //LogMessage
    }

    if (m_fCautionary)
        m_glyphs[i++] = k_glyph_close_cautionary_accidental;

    m_numGlyphs = i;
}

//---------------------------------------------------------------------------------------
void AccidentalsEngraver::create_container_shape(ImoObj* pCreatorImo, UPoint pos)
{
    m_pContainer = LOMSE_NEW GmoShapeAccidentals(pCreatorImo, 0, pos, Color(0,0,0));
}

//---------------------------------------------------------------------------------------
void AccidentalsEngraver::add_glyphs_to_container_shape(UPoint pos)
{
    LUnits x = pos.x;
    for (int i=0; i < m_numGlyphs; ++i)
    {
        int iGlyph = m_glyphs[i];
        LUnits y = pos.y + glyph_offset(iGlyph);
        GmoShape* pShape = LOMSE_NEW GmoShapeAccidental(m_pCreatorImo, 0, iGlyph, UPoint(x, y),
                                                  Color(0,0,0), m_libraryScope,
                                                  m_fontSize);
        m_pContainer->add(pShape);
        x += (pShape->get_left() - x) + pShape->get_width();
        x += m_pMeter->tenths_to_logical(LOMSE_SPACE_BETWEEN_ACCIDENTALS,
                                         m_iInstr, m_iStaff);
    }
}

//---------------------------------------------------------------------------------------
LUnits AccidentalsEngraver::glyph_offset(int iGlyph)
{
    Tenths offset = glyphs_lmbasic2[iGlyph].GlyphOffset + 40.0f;
    return m_pMeter->tenths_to_logical(offset, m_iInstr, m_iStaff);
}

//---------------------------------------------------------------------------------------
double AccidentalsEngraver::determine_font_size()
{
    //TODO
    return 21.0 * m_pMeter->line_spacing_for_instr_staff(m_iInstr, m_iStaff) / 180.0;
}


}  //namespace lomse