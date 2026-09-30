#ifndef COSTWAVES_QUANTITIES_DIALOG_HPP
#define COSTWAVES_QUANTITIES_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Fenêtre « Quantitatif » — contrôle et calcul des quantités AVANT export
// vers CostWaves. LE MOTEUR SUIT LA CORRESPONDANCE : pour chaque élément il
// cherche la règle dans la bibliothèque (fenêtre Matériaux, composites et
// profils) — un composite/profil avec article est calculé pour LUI-MÊME
// (mode Élément de la règle) ou PAR SES COUCHES (mode Composants), un
// composite/profil sans article est « quantifié par matériau décomposé »
// (ses couches, articles des règles matériaux) ; jamais les deux à la fois :
//  - tableau : Article | Source | Unité | Mode calcul | Quantité — une ligne
//    par article CostWaves EFFECTIVEMENT quantifié dans la maquette ;
//  - Source CLIQUABLE : la colonne affiche la chaîne de traçabilité complète
//    (Article → Mapping → Source Archicad → Paramètres → Mode de calcul →
//    Quantité calculée / retenue) — comprendre POURQUOI une quantité a été
//    obtenue avant de l'envoyer ;
//  - les RÉGLAGES DE CALCUL (unité, mode Brute/Conditionnelle/Nette,
//    déductions) se règlent désormais dans les FENÊTRES DE CORRESPONDANCE
//    (colonnes éditables de la ligne, tous les types) — le panneau du
//    Quantitatif les AFFICHE en lecture seule ;
//  - la quantité retenue reste corrigible manuellement (« Retenir » /
//    « Réinitialiser ») — la quantité calculée d'origine est TOUJOURS
//    conservée et affichée ;
//  - « Recalculer » relit la maquette ; les corrections manuelles sont
//    conservées par article.
class QuantitiesDialog final :	public DG::ModalDialog,
					public DG::ButtonItemObserver,
					public DG::ListBoxObserver,
					public DG::PopUpObserver
{
public:
	enum ItemIds {
		InfoTextId			= 1,
		SystemLabelId		= 2,
		SystemPopupId		= 3,
		ListId				= 4,
		SettingsTextId		= 5,
		CalcLabelId			= 6,
		CalcValueTextId		= 7,
		RetainedLabelId		= 8,
		RetainedEditId		= 9,
		ApplyRetainedId		= 10,
		ResetRetainedId		= 11,
		HintTextId			= 12,
		RecalcButtonId		= 13,
		CloseButtonId		= 14
	};

	QuantitiesDialog ();

private:
	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;
	virtual void	ListBoxClicked (const DG::ListBoxClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	// Lecture de la maquette (scan complet, règles appliquées).
	void	RunScan ();
	// Recalcule les lignes depuis les rows (conserve les réglages/corrections).
	void	RebuildLines ();
	// Remplit le tableau (conserve la sélection).
	void	FillTable ();
	// Met à jour le panneau de calcul depuis la ligne sélectionnée.
	void	UpdateDetailPanel ();
	// Traçabilité complète d'une ligne (colonne Source cliquable).
	void	ShowTraceability (const CWQuantityLine& line);
	// Corrige manuellement la quantité retenue de la ligne sélectionnée.
	void	ApplyRetainedValue ();

	CWQuantityLine*		SelectedLine ();
	const CWQuantityLine* SelectedLine () const;
	API_Guid			CurrentSystemGuid () const;

	DG::LeftText		infoText;
	DG::LeftText		systemLabel;
	DG::PopUp			systemPopup;
	DG::MultiSelListBox	list;
	DG::LeftText		settingsText;	// réglages de la correspondance (lecture seule)
	DG::LeftText		calcLabel;
	DG::LeftText		calcValueText;
	DG::LeftText		retainedLabel;
	DG::TextEdit		retainedEdit;
	DG::Button			applyRetainedButton;
	DG::Button			resetRetainedButton;
	DG::LeftText		hintText;
	DG::Button			recalcButton;
	DG::Button			closeButton;

	GS::Array<CWSystemInfo>	systems;
	GS::Array<CWArticle>	articles;
	GS::Array<CWMapRule>	rules;
	GS::Array<CWElementRow>	rows;
	CWScanReport			report;
	GS::Array<CWQuantityLine>	lines;

	short	selectedRow = 0;
	bool	isFilling = false;
};

} // namespace CostWaves

#endif // COSTWAVES_QUANTITIES_DIALOG_HPP
