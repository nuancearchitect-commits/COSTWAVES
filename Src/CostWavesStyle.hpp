#ifndef COSTWAVES_STYLE_HPP
#define COSTWAVES_STYLE_HPP

// Kit de style CostWaves — jetons visuels de la maquette costwaves-v4
// (thème clair) appliqués aux fenêtres DG de l'add-on. La maquette sert
// de référence pour les COULEURS et la HIÉRARCHIE (bande sombre « rail »,
// accent bleu, libellés gris, pastilles « chips »), pas pour la copie de
// ses écrans : les fenêtres restent des dialogues natifs Archicad.
//
//   bande sombre (rail #101826)  → titre de la palette
//   accent #155eef               → valeurs clés, libellés de section, chips
//   ink #132033 / ink-2 #3d4e63  → textes principaux, libellés de champ
//   ink-3 #6a7b90                → aide, descriptions, cellules vides « — »
//   ok #027a48                   → valeurs définies (validées)
//   soft #e8f0fe                 → fond des chips accent

#include "DGModule.hpp"

namespace CostWaves {
namespace CostWavesStyle {

// --- Couleurs (maquette V4, thème clair) -----------------------------------

inline Gfx::Color Accent ()		{ return Gfx::Color (0x15, 0x5E, 0xEF); }	// accent
inline Gfx::Color Ink ()		{ return Gfx::Color (0x13, 0x20, 0x33); }	// texte principal
inline Gfx::Color Ink2 ()		{ return Gfx::Color (0x3D, 0x4E, 0x63); }	// libellés de champ
inline Gfx::Color Ink3 ()		{ return Gfx::Color (0x6A, 0x7B, 0x90); }	// aide, vides
inline Gfx::Color Rail ()		{ return Gfx::Color (0x10, 0x18, 0x26); }	// bande sombre
inline Gfx::Color Soft ()		{ return Gfx::Color (0xE8, 0xF0, 0xFE); }	// fond chip accent
inline Gfx::Color Wash ()		{ return Gfx::Color (0xF4, 0xF7, 0xFB); }	// fond discret
inline Gfx::Color Ok ()			{ return Gfx::Color (0x02, 0x7A, 0x48); }	// défini / validé
inline Gfx::Color Warn ()		{ return Gfx::Color (0xB5, 0x47, 0x08); }	// attention
inline Gfx::Color Bad ()		{ return Gfx::Color (0xB4, 0x23, 0x18); }	// erreur
inline Gfx::Color Paper ()		{ return Gfx::Color (0xFF, 0xFF, 0xFF); }	// blanc

// --- Habillage des textes ----------------------------------------------------

// Bande sombre « rail » : titre blanc en gras sur fond bleu nuit —
// l'identité CostWaves de la maquette (barre latérale sombre).
inline void	ApplyBand (DG::LeftText& item)
{
	item.SetBackgroundColor (Rail ());
	item.SetTextColor (Paper ());
	item.SetFontStyle (DG::Font::Bold);
}

// Libellé de section (majuscules) : accent bleu, gras.
inline void	ApplySectionLabel (DG::LeftText& item)
{
	item.SetTextColor (Accent ());
	item.SetFontStyle (DG::Font::Bold);
}

// Libellé de champ (« Type : », « Classification : »…) : ink-2, gras.
inline void	ApplyFieldLabel (DG::LeftText& item)
{
	item.SetTextColor (Ink2 ());
	item.SetFontStyle (DG::Font::Bold);
}

// Texte d'aide / description : gris ink-3.
inline void	ApplyHelp (DG::LeftText& item)
{
	item.SetTextColor (Ink3 ());
}

// Ligne d'état en « chip » : fond soft, texte accent, gras.
inline void	ApplyStatusChip (DG::LeftText& item)
{
	item.SetBackgroundColor (Soft ());
	item.SetTextColor (Accent ());
	item.SetFontStyle (DG::Font::Bold);
}

// --- Cellules de listes -------------------------------------------------------

// Valeur porteuse (article, classe) : accent bleu.
inline void	CellAccent (DG::ListBox& list, short item, short tab)
{
	list.SetTabItemColor (item, tab, Accent ());
}

// Valeur définie (valeur clé choisie) : vert « ok ».
inline void	CellOk (DG::ListBox& list, short item, short tab)
{
	list.SetTabItemColor (item, tab, Ok ());
}

// Cellule vide ou secondaire (« — », aide, décomposé) : gris ink-3.
inline void	CellMuted (DG::ListBox& list, short item, short tab)
{
	list.SetTabItemColor (item, tab, Ink3 ());
}

// Pastille « chip » dans une cellule (ex. colonne Type) : fond soft +
// texte accent — le type attendu se repère d'un coup d'œil.
inline void	CellChip (DG::ListBox& list, short item, short tab)
{
	list.SetTabItemBackgroundColor (item, tab, Soft ());
	list.SetTabItemColor (item, tab, Accent ());
}

} // namespace CostWavesStyle
} // namespace CostWaves

#endif // COSTWAVES_STYLE_HPP
