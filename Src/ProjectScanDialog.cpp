#include "CostWavesPrecompiledHeader.hpp"

#include "ProjectScanDialog.hpp"

#include "ArticleEditDialog.hpp"
#include "ArticleManager.hpp"
#include "LayersDialog.hpp"
#include "ModelReader.hpp"
#include "RuleLibrary.hpp"

namespace CostWaves {

namespace {

GS::UniString FR (const char* utf8Text)
{
	return GS::UniString (utf8Text, CC_UTF8);
}

// Fenêtre « Assigner un article » : article de la base (ou création à la
// volée) pour la structure donnée.
class AssignArticleDialog final :	public DG::ModalDialog,
									public DG::ButtonItemObserver
{
public:
	AssignArticleDialog (const GS::UniString& structureLabel,
						 const GS::Array<CWArticle>& inArticles)
		:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_ASSIGN, ACAPI_GetOwnResModule ()),
			infoText (GetReference (), 1),
			articleLabel (GetReference (), 2),
			articlePopup (GetReference (), 3),
			assignButton (GetReference (), 4),
			cancelButton (GetReference (), 5),
			createButton (GetReference (), 6),
			articles (inArticles)
	{
		infoText.SetText (structureLabel + FR (" — choisissez l'article CostWaves à appliquer."));

		FillPopup ();

		assignButton.Attach (*this);
		cancelButton.Attach (*this);
		createButton.Attach (*this);
	}

	bool					IsAccepted () const { return accepted; }
	const GS::UniString&	GetSelectedArticleId () const { return selectedArticleId; }
	const GS::Array<CWArticle>& GetArticles () const { return articles; }

private:
	void	FillPopup ()
	{
		while (articlePopup.GetItemCount () > 0)
			articlePopup.DeleteItem (1);

		for (UIndex a = 0; a < articles.GetSize (); ++a) {
			GS::UniString label = articles[a].id + FR (" — ") + articles[a].name;
			if (!articles[a].unit.IsEmpty ())
				label += FR (" (") + articles[a].unit + FR (")");
			articlePopup.AppendItem ();
			articlePopup.SetItemText (articlePopup.GetItemCount (), label);
		}
		if (articlePopup.GetItemCount () > 0)
			articlePopup.SelectItem (1);
		else
			infoText.SetText (FR ("Aucun article dans la base — créez-en un ou importez la base ")
							  + FR ("depuis le gestionnaire de correspondances."));
	}

	virtual void	ButtonClicked (const DG::ButtonClickEvent& ev) override
	{
		if (ev.GetSource () == &assignButton) {
			const short selection = articlePopup.GetSelectedItem ();
			if (selection >= 1 && static_cast<UIndex> (selection) <= articles.GetSize ()) {
				selectedArticleId = articles[static_cast<UIndex> (selection) - 1].id;
				accepted = true;
				PostCloseRequest (DG::ModalDialog::Accept);
			}
		} else if (ev.GetSource () == &createButton) {
			ArticleEditDialog editDialog;
			editDialog.Invoke ();
			if (editDialog.IsAccepted ()) {
				const CWArticle article = editDialog.GetArticle ();
				GS::UniString saveError;
				if (!ArticleManager::SaveLocalArticle (article, saveError))
					DG::WarningAlert (FR ("Article créé, mais non enregistré."), saveError, FR ("OK"));
				articles.Push (article);
				FillPopup ();
				// Sélectionner l'article créé.
				if (articlePopup.GetItemCount () > 0)
					articlePopup.SelectItem (articlePopup.GetItemCount ());
			}
		} else if (ev.GetSource () == &cancelButton) {
			PostCloseRequest (DG::ModalDialog::Cancel);
		}
	}

	DG::LeftText	infoText;
	DG::LeftText	articleLabel;
	DG::PopUp		articlePopup;
	DG::Button		assignButton;
	DG::Button		cancelButton;
	DG::Button		createButton;

	GS::Array<CWArticle>	articles;
	GS::UniString			selectedArticleId;
	bool					accepted = false;
};

// Rang de tri : ⚠ d'abord, puis Ignoré, puis OK.
short StatusRank (bool ignored, bool hasRule)
{
	if (ignored)
		return 1;
	return hasRule ? 2 : 0;
}

} // namespace


ProjectScanDialog::ProjectScanDialog ()
	:	DG::ModalDialog (ACAPI_GetOwnResModule (), ID_ADDON_DLG_SCAN, ACAPI_GetOwnResModule ()),
		infoText (GetReference (), InfoTextId),
		table (GetReference (), TableId),
		refreshButton (GetReference (), RefreshButtonId),
		assignButton (GetReference (), AssignButtonId),
		ignoreButton (GetReference (), IgnoreButtonId),
		createArticleButton (GetReference (), CreateArticleButtonId),
		statusText (GetReference (), StatusTextId),
		closeButton (GetReference (), CloseButtonId)
{
	infoText.SetText (FR ("Structures détectées dans le projet. ⚠ = sans article : assignez-en un ")
					  + FR ("ou ignorez la structure — la décision devient une règle réutilisable."));

	refreshButton.Attach (*this);
	assignButton.Attach (*this);
	ignoreButton.Attach (*this);
	createArticleButton.Attach (*this);
	closeButton.Attach (*this);
	table.Attach (*this);

	ScanProject ();
}


void ProjectScanDialog::ScanProject ()
{
	isFilling = true;

	// Bibliothèque de règles + base d'articles.
	rules.Clear ();
	GS::UniString rulesError;
	RuleLibrary::LoadRules (rules, rulesError);		// absente = bibliothèque vide

	articles.Clear ();
	GS::UniString baseError;
	ArticleManager::LoadArticleBase (articles, baseError);

	// Éléments placés.
	scanRows.Clear ();
	GS::Array<API_Guid> elemList;
	if (ACAPI_Element_GetElemList (API_ZombieElemID, &elemList) != NoError) {
		FillTable ();
		isFilling = false;
		statusText.SetText (FR ("Impossible de lire les éléments du projet."));
		return;
	}

	for (UIndex i = 0; i < elemList.GetSize (); ++i) {
		API_Elem_Head header;
		BNZeroMemory (&header, sizeof (header));
		header.guid = elemList[i];
		if (ACAPI_Element_GetHeader (&header) != NoError)
			continue;

		// Types porteurs d'une structure : murs, dalles, toitures, coquilles,
		// poteaux, poutres, objets, lampes, portes, fenêtres.
		switch (header.type.typeID) {
			case API_WallID: case API_SlabID: case API_RoofID: case API_ShellID:
			case API_ColumnID: case API_BeamID:
			case API_ObjectID: case API_LampID: case API_DoorID: case API_WindowID:
				break;
			default:
				continue;
		}

		CWStructureType structureType = CWStructureType::Composite;
		GS::UniString structureName;
		if (!ModelReader::GetElementStructure (elemList[i], header.type.typeID,
											   structureType, structureName))
			continue;

		// Regroupement par (type, nom).
		ScanRow* row = nullptr;
		for (UIndex r = 0; r < scanRows.GetSize (); ++r) {
			if (scanRows[r].type == structureType && scanRows[r].name == structureName) {
				row = &scanRows[r];
				break;
			}
		}
		if (row == nullptr) {
			ScanRow newRow;
			newRow.type = structureType;
			newRow.name = structureName;
			scanRows.Push (newRow);
			row = &scanRows[scanRows.GetSize () - 1];
		}
		++row->elementCount;
		if (row->type == CWStructureType::Profile)
			row->elementGuids.Push (elemList[i]);		// matériaux découverts à la demande
	}

	// Évaluation des règles par structure.
	for (UIndex r = 0; r < scanRows.GetSize (); ++r) {
		ScanRow& row = scanRows[r];
		const CWMapRule* rule = RuleLibrary::FindRule (rules, row.type, row.name);

		if (rule != nullptr && rule->ignored) {
			row.ignored = true;
			continue;
		}

		if (row.type == CWStructureType::LibraryPart || row.type == CWStructureType::BuildingMaterial) {
			// Objet / matériau : la règle porte l'article directement.
			if (rule != nullptr && !rule->articleId.IsEmpty ()) {
				row.hasRule = true;
				row.articleId = rule->articleId;
			}
			continue;
		}

		// Composite / profil.
		if (rule != nullptr && rule->mode == CWQuantMode::Element && !rule->articleId.IsEmpty ()) {
			row.hasRule = true;
			row.articleId = rule->articleId;
			continue;
		}
		if (rule != nullptr && rule->mode == CWQuantMode::Component) {
			// Mode « ses couches » : chaque matériau doit avoir un article.
			row.byLayers = true;
			if (row.type == CWStructureType::Composite) {
				row.layersKnown = ModelReader::GetStructureLayerMaterialNames (row.name, row.layerMaterials);
			} else {
				ModelReader::CollectSkinMaterialNames (row.elementGuids, row.layerMaterials);
				row.layersKnown = !row.layerMaterials.IsEmpty ();
			}
			row.hasRule = row.layersKnown && !row.layerMaterials.IsEmpty ();
			if (row.hasRule) {
				for (UIndex m = 0; m < row.layerMaterials.GetSize (); ++m) {
					const CWMapRule* materialRule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial,
																		  row.layerMaterials[m]);
					if (materialRule == nullptr || materialRule->articleId.IsEmpty () || materialRule->ignored) {
						row.hasRule = false;
						break;
					}
				}
			}
		}
		// Pas de règle : reste ⚠.
	}

	// Tri : ⚠ d'abord, puis Ignoré, puis OK (stable par nom).
	for (UIndex a = 0; a + 1 < scanRows.GetSize (); ++a) {
		for (UIndex b = a + 1; b < scanRows.GetSize (); ++b) {
			const short rankA = StatusRank (scanRows[a].ignored, scanRows[a].hasRule);
			const short rankB = StatusRank (scanRows[b].ignored, scanRows[b].hasRule);
			if (rankB < rankA) {
				ScanRow temp = scanRows[a];
				scanRows[a] = scanRows[b];
				scanRows[b] = temp;
			}
		}
	}

	selectedRowIndex = 0;
	FillTable ();
	UpdateStatus ();
	isFilling = false;
}


void ProjectScanDialog::FillTable ()
{
	const short columnCount = 5;
	table.SetHeaderItemCount (columnCount);
	table.SetTabFieldCount (columnCount);
	table.SetHeaderItemText (1, FR ("Statut"));
	table.SetHeaderItemText (2, FR ("Type"));
	table.SetHeaderItemText (3, FR ("Structure"));
	table.SetHeaderItemText (4, FR ("Éléments"));
	table.SetHeaderItemText (5, FR ("Article"));

	const short widths[5] = { 170, 150, 200, 70, 180 };
	short position = 0;
	for (short i = 1; i <= columnCount; ++i) {
		table.SetHeaderItemSize (i, widths[i - 1]);
		table.SetHeaderItemSizeableFlag (i, true);
		table.SetTabFieldProperties (i, position, static_cast<short> (position + widths[i - 1]),
									 DG::ListBox::Left, DG::ListBox::MiddleTruncate, i > 1);
		position = static_cast<short> (position + widths[i - 1]);
	}

	while (table.GetItemCount () > 0)
		table.DeleteItem (1);

	for (UIndex r = 0; r < scanRows.GetSize (); ++r) {
		const ScanRow& row = scanRows[r];
		table.AppendItem ();
		const short item = table.GetItemCount ();
		table.SetTabItemText (item, 2, RuleLibrary::StructureTypeName (row.type));
		table.SetTabItemText (item, 3, row.name);
		table.SetTabItemText (item, 4, GS::ToUniString (std::to_wstring (row.elementCount)));

		if (row.ignored) {
			table.SetTabItemText (item, 1, FR ("Ignoré"));
			table.SetTabItemText (item, 5, FR ("—"));
		} else if (row.hasRule && row.byLayers) {
			table.SetTabItemText (item, 1, FR ("✓ par couches"));
			table.SetTabItemText (item, 5, FR ("(matériaux)"));
		} else if (row.hasRule) {
			table.SetTabItemText (item, 1, FR ("✓"));
			const CWArticle* article = ArticleManager::FindArticle (articles, row.articleId);
			table.SetTabItemText (item, 5, article != nullptr
				? row.articleId + FR (" — ") + article->name
				: row.articleId);
		} else if (row.byLayers) {
			// Mode couches avec des matériaux sans article.
			USize missing = 0;
			for (UIndex m = 0; m < row.layerMaterials.GetSize (); ++m) {
				const CWMapRule* materialRule = RuleLibrary::FindRule (rules, CWStructureType::BuildingMaterial,
																	  row.layerMaterials[m]);
				if (materialRule == nullptr || materialRule->articleId.IsEmpty () || materialRule->ignored)
					++missing;
			}
			table.SetTabItemText (item, 1, FR ("⚠ matériaux à définir"));
			table.SetTabItemText (item, 5, missing > 0
				? GS::ToUniString (std::to_wstring (static_cast<int> (missing))) + FR (" couche(s) sans article")
				: FR ("couches non lisibles"));
		} else {
			table.SetTabItemText (item, 1, FR ("⚠ à définir"));
			table.SetTabItemText (item, 5, FR ("—"));
		}
	}

	if (table.GetItemCount () > 0) {
		table.SelectItem (1);
		selectedRowIndex = 1;
	}
}


ProjectScanDialog::ScanRow* ProjectScanDialog::SelectedRow ()
{
	if (selectedRowIndex < 1 || static_cast<UIndex> (selectedRowIndex) > scanRows.GetSize ())
		return nullptr;
	return &scanRows[static_cast<UIndex> (selectedRowIndex) - 1];
}


void ProjectScanDialog::AssignSelected ()
{
	ScanRow* row = SelectedRow ();
	if (row == nullptr) {
		DG::WarningAlert (FR ("Aucune structure sélectionnée."),
						  FR ("Sélectionnez une ligne du tableau."), FR ("OK"));
		return;
	}
	if (row->ignored) {
		statusText.SetText (FR ("Structure ignorée — supprimez la règle dans le gestionnaire pour la réactiver."));
		return;
	}

	// Composite/profil en mode « ses couches » : donner des articles aux
	// matériaux qui n'en ont pas.
	if (row->byLayers && row->layersKnown && !row->layerMaterials.IsEmpty ()) {
		LayersDialog layersDialog (row->name, row->layerMaterials, articles, rules);
		layersDialog.Invoke ();
		SaveLibrary ();
		ScanProject ();
		return;
	}

	// Cas général : assigner un article à la structure (règle « lui-même »).
	AssignArticleDialog assignDialog (RuleLibrary::StructureTypeName (row->type) + FR (" — ") + row->name,
									  articles);
	assignDialog.Invoke ();
	if (!assignDialog.IsAccepted ())
		return;

	articles = assignDialog.GetArticles ();		// articles créés à la volée

	CWMapRule rule;
	rule.structureType = row->type;
	rule.structureName = row->name;
	rule.articleId = assignDialog.GetSelectedArticleId ();
	rule.mode = CWQuantMode::Element;

	bool replaced = false;
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType == rule.structureType && rules[r].structureName == rule.structureName) {
			rules[r] = rule;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		rules.Push (rule);

	SaveLibrary ();
	ScanProject ();
}


void ProjectScanDialog::IgnoreSelected ()
{
	ScanRow* row = SelectedRow ();
	if (row == nullptr) {
		DG::WarningAlert (FR ("Aucune structure sélectionnée."),
						  FR ("Sélectionnez une ligne du tableau."), FR ("OK"));
		return;
	}
	if (row->ignored)
		return;

	CWMapRule rule;
	rule.structureType = row->type;
	rule.structureName = row->name;
	rule.mode = CWQuantMode::Element;
	rule.ignored = true;

	bool replaced = false;
	for (UIndex r = 0; r < rules.GetSize (); ++r) {
		if (rules[r].structureType == rule.structureType && rules[r].structureName == rule.structureName) {
			rules[r] = rule;
			replaced = true;
			break;
		}
	}
	if (!replaced)
		rules.Push (rule);

	SaveLibrary ();
	ScanProject ();
}


void ProjectScanDialog::CreateArticle ()
{
	ArticleEditDialog editDialog;
	editDialog.Invoke ();
	if (!editDialog.IsAccepted ())
		return;

	const CWArticle article = editDialog.GetArticle ();
	GS::UniString saveError;
	if (!ArticleManager::SaveLocalArticle (article, saveError))
		DG::WarningAlert (FR ("Article créé, mais non enregistré."), saveError, FR ("OK"));

	articles.Push (article);
	UpdateStatus ();
}


void ProjectScanDialog::SaveLibrary ()
{
	GS::UniString error;
	if (!RuleLibrary::SaveRules (rules, error))
		DG::ErrorAlert (FR ("Échec de l'enregistrement de la bibliothèque."), error, FR ("OK"));
}


void ProjectScanDialog::UpdateStatus ()
{
	USize unassigned = 0;
	USize ignored = 0;
	USize ok = 0;
	for (UIndex r = 0; r < scanRows.GetSize (); ++r) {
		if (scanRows[r].ignored)
			++ignored;
		else if (scanRows[r].hasRule)
			++ok;
		else
			++unassigned;
	}

	statusText.SetText (GS::ToUniString (std::to_wstring (static_cast<int> (unassigned)))
						+ FR (" structure(s) ⚠ à configurer · ")
						+ GS::ToUniString (std::to_wstring (static_cast<int> (ignored)))
						+ FR (" ignorée(s) · ")
						+ GS::ToUniString (std::to_wstring (static_cast<int> (ok)))
						+ FR (" avec article"));
}


void ProjectScanDialog::ListBoxSelectionChanged (const DG::ListBoxSelectionEvent& ev)
{
	if (ev.GetSource () != &table || isFilling)
		return;

	selectedRowIndex = table.GetSelectedItem ();
}


void ProjectScanDialog::ButtonClicked (const DG::ButtonClickEvent& ev)
{
	if (ev.GetSource () == &refreshButton) {
		ScanProject ();
	} else if (ev.GetSource () == &assignButton) {
		AssignSelected ();
	} else if (ev.GetSource () == &ignoreButton) {
		IgnoreSelected ();
	} else if (ev.GetSource () == &createArticleButton) {
		CreateArticle ();
	} else if (ev.GetSource () == &closeButton) {
		PostCloseRequest (DG::ModalDialog::Accept);
	}
}

} // namespace CostWaves
