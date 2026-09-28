#ifndef COSTWAVES_COSTWAVES_DIALOG_HPP
#define COSTWAVES_COSTWAVES_DIALOG_HPP

#include "ACAPinc.h"
#include "DGModule.hpp"

#include "ResourceIds.hpp"
#include "DataTypes.hpp"

namespace CostWaves {

// Palette principale (modeless, phase 5 — ne bloque ni la navigation ni la
// sélection dans Archicad) :
//  - choix du système de classification
//  - tableau : Type | GUID | ID élément | Étage | Classe | Quantités
//  - panneau de détails (toutes les quantités + propriétés de composant)
//  - lecture de la sélection courante (case à cocher, suivie en direct)
//  - articles CostWaves : import JSON, création de la classification,
//    affectation d'un article à l'élément sélectionné
//  - création d'un matériau lié à une classe (nouvelle ou existante)
//  - « Créer un ensemble » / « Créer un groupe » (groupes numérotés) :
//    sélection d'éléments dans le plan, puis choix de l'article
//  - exports JSON / CSV enrichis de l'article
class CostWavesDialog final :	public DG::Palette,
								public DG::PanelObserver,
								public DG::ButtonItemObserver,
								public DG::PopUpObserver,
								public DG::ListBoxObserver,
								public DG::SearchEditObserver
{
public:
	enum DialogResourceIds {
		DialogResourceId	= ID_ADDON_DLG,

		SystemLabelId		= 1,
		SystemPopupId		= 2,
		RefreshButtonId		= 3,
		StatusTextId		= 4,
		TableId				= 5,
		DetailsGroupId		= 6,
		DetailText1Id		= 7,
		DetailText2Id		= 8,
		DetailText3Id		= 9,
		DetailText4Id		= 10,
		DetailText5Id		= 11,
		ExportJsonButtonId	= 12,
		ExportCsvButtonId	= 13,
		CloseButtonId		= 14,
		SelectionCheckId	= 15,
		ArticlesLabelId		= 16,
		ArticlePopupId		= 17,
		AssignButtonId		= 18,
		ImportButtonId		= 19,
		CreateClassButtonId	= 20,
		ArticlesInfoId		= 21,
		SearchLabelId		= 22,
		SearchEditId		= 23,
		CreateMaterialsButtonId	= 24,
		GroupButtonId		= 25,
		UngroupButtonId		= 26,
		SummaryButtonId		= 27,
		CreateGroupButtonId	= 28
	};

	// Singleton : la palette vit aussi longtemps que l'add-on.
	static bool				HasInstance ();
	static CostWavesDialog&	Instance ();
	static GSErrCode			RegisterPalette ();		// ACAPI_RegisterModelessWindow (Initialize)

	// Callback de changement de sélection Archicad (installé via
	// ACAPI_Notification_CatchSelectionChange) : actualise la palette si
	// elle est visible et que « Sélection uniquement » est cochée.
	static GSErrCode		SelectionChangeHandler (const API_Neig* selElemNeig);

	void					ShowPalette ();
	void					HidePalette ();

	// La palette suit-elle la sélection du plan ? (case « Sélection uniquement »)
	bool					WantsSelectionFollow () const;
	// Actualisation déclenchée de l'extérieur (callback de sélection) :
	// sans effet si une boîte modale est ouverte ou si une lecture est en cours.
	void					RefreshFromSelectionChange ();

	// Public : la suppression est opérée par GS::Ref (singleton statique).
	~CostWavesDialog ();

private:
	CostWavesDialog ();

	// DG::PanelObserver
	virtual void	PanelResized (const DG::PanelResizeEvent& ev) override;
	virtual void	PanelCloseRequested (const DG::PanelCloseRequestEvent& ev, bool* accepted) override;

	// DG::ButtonItemObserver
	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override;

	// DG::PopUpObserver
	virtual void	PopUpChanged (const DG::PopUpChangeEvent& ev) override;

	// DG::ListBoxObserver
	virtual void	ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev) override;
	virtual void	ListBoxHeaderItemClicked (const DG::ListBoxHeaderItemClickEvent& ev) override;

	// DG::SearchEditObserver
	virtual void	SearchTextChanged (const DG::SearchEditChangeEvent& ev) override;

	void	LoadSystems ();
	void	RefreshData ();
	void	FillTable ();
	void	UpdateDetails (short listItem);
	void	UpdateStatus ();
	void	Export (bool jsonFormat);
	void	SetDetailLine (short lineIndex, const GS::UniString& text);
	void	ClearDetails ();

	// Articles CostWaves (phase 2).
	void	ReloadArticlesFromSystem ();	// articles = items du système choisi
	void	LoadArticlesPopup ();		// (re)remplit le popup des articles
	void	ImportArticles ();			// import JSON (boîte de dialogue fichier)
	void	CreateClassification ();		// crée/maj le système "CostWaves" (annulable)
	void	AssignCurrentArticle ();	// affecte l'article aux éléments sélectionnés
	void	CreateMaterials ();			// fenêtre « Créer le matériau… » (phase 5)

	// Ensembles / groupes CostWaves (phases 4-5).
	// Sélection d'éléments dans le PLAN, puis choix de l'article :
	//  - numbered = false : ensemble (CW-E-…), facturé comme une ligne ;
	//  - numbered = true  : groupe numéroté (CW-N-…), 1 par groupe dans le métré.
	void	CreateEnsembleOrGroup (bool numbered);
	void	UngroupSelected ();		// dissout le(s) ensemble(s)/groupe(s) sélectionné(s)
	void	ShowSummary ();			// fenêtre « Récapitulatif par article »

	// Résout l'item de classification de l'article (système « CostWaves » en
	// priorité, sinon le système courant). Retourne false si introuvable.
	bool	ResolveArticleTarget (const GS::UniString& articleId,
								  API_Guid& outSystemGuid, API_Guid& outItemGuid);

	// L'élément correspond-il au filtre de recherche courant ?
	bool	ElementMatchesFilter (const CWElementRow& element) const;

	// Tri du tableau par colonne (les composants restent rattachés à leur
	// élément). Applique le tri courant à "rows".
	void	SortRows ();

	// Texte de la colonne « Facturé » (élément/ensemble/groupe, puis skin).
	GS::UniString	BilledText (const CWElementRow& row) const;
	GS::UniString	SkinBilledText (const CWComponentRow& skin) const;

	// Ligne d'affichage : référence vers un élément, un de ses composants,
	// ou un membre d'ensemble (phase 4).
	struct DisplayRow {
		RowKind	kind = RowKind::Element;
		UIndex	elementIndex = 0;
		UIndex	componentIndex = 0;
		UIndex	memberRowIndex = 0;	// GroupMember : index de la ligne du membre
	};

	DG::PopUp			systemPopup;
	DG::Button			refreshButton;
	DG::CheckBox		selectionCheck;
	DG::LeftText		statusText;
	DG::MultiSelListBox	table;
	DG::GroupBox		detailsGroup;
	DG::LeftText		detail1;
	DG::LeftText		detail2;
	DG::LeftText		detail3;
	DG::LeftText		detail4;
	DG::LeftText		detail5;
	DG::PopUp			articlePopup;
	DG::Button			assignButton;
	DG::Button			importButton;
	DG::Button			createClassButton;
	DG::Button			createMaterialsButton;
	DG::Button			groupButton;
	DG::Button			createGroupButton;
	DG::Button			ungroupButton;
	DG::Button			summaryButton;
	DG::LeftText		articlesInfo;
	DG::LeftText		searchLabel;
	DG::SearchEdit		searchEdit;
	DG::Button			exportJsonButton;
	DG::Button			exportCsvButton;
	DG::Button			closeButton;

	GS::Array<CWSystemInfo>	systems;
	API_Guid			selectedSystem = APINULLGuid;
	API_Guid			elemIdPropGuid = APINULLGuid;
	API_Guid			groupPropGuid = APINULLGuid;	// CW_Group_ID (phase 4)
	GS::UniString			elemIdPropNote;

	GS::Array<CWArticle>	articles;			// catalogue courant
	bool					articlesImported = false;	// true = import JSON (prioritaire)
	GS::UniString			articlesSourceName;	// nom du système ou du fichier source

	GS::Array<CWElementRow>	rows;
	CWScanReport			report;
	GS::Array<DisplayRow>	displayRows;

	GS::UniString			searchFilter;		// filtre de recherche courant

	// Libellés des colonnes de quantités (une colonne par type, reconstruits
	// à chaque remplissage du tableau).
	GS::Array<GS::UniString>	quantityColumnLabels;

	short					sortColumn = 0;		// colonne de tri (0 = aucun, sinon 1..6)
	bool					sortAscending = true;

	bool					isFilling = false;
	bool					inModalDialog = false;	// une boîte modale est ouverte
	bool					refreshing = false;		// lecture en cours (anti-réentrance)

	// Singleton + identité de la palette auprès d'Archicad.
	static GS::Ref<CostWavesDialog>	instance;
	static const GS::Guid			paletteGuid;
	static GSErrCode				PaletteControlCallBack (Int32 paletteId, API_PaletteMessageID messageID, GS::IntPtr param);
};

} // namespace CostWaves

#endif // COSTWAVES_COSTWAVES_DIALOG_HPP
