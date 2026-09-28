#ifndef COSTWAVES_ARTICLE_MANAGER_HPP
#define COSTWAVES_ARTICLE_MANAGER_HPP

#include "ACAPinc.h"
#include "DataTypes.hpp"

namespace CostWaves {

// Gestion des articles CostWaves {id, name, unit} (phase 2) :
//  - import depuis un fichier JSON (UTF-8) : [{"id":"...","name":"...","unit":"..."}, ...]
//    ou {"articles":[...]}
//  - collecte des articles depuis les items d'un système de classification
//  - création (annulable) du système "CostWaves" avec un item par article
//  - affectation (annulable) d'un item de classification à un élément
//     + écriture de la propriété CW_Article_ID sur l'élément
//  - création des matériaux de construction : un matériau par article,
//    son item de classification, et l'affectation item -> matériau
class ArticleManager {
public:
	// Nom du système de classification créé par l'add-on.
	static const char* CostWavesSystemName ();

	// Nom du groupe de propriétés et de la propriété texte créés par l'add-on.
	static const char* PropertyGroupName ();
	static const char* ArticleIdPropertyName ();

	// Importe les articles d'un fichier JSON. Retourne false + outError en cas
	// d'échec (fichier illisible, JSON invalide, aucun article).
	static bool	ImportFromJsonFile (const GS::UniString& path, GS::Array<CWArticle>& outArticles,
									 GS::UniString& outError);

	// Articles = items du système de classification donné (unit vide).
	static bool	CollectFromClassification (const API_Guid& systemGuid, GS::Array<CWArticle>& outArticles);

	// Guid du système nommé "CostWaves" (APINULLGuid s'il n'existe pas).
	static API_Guid	FindCostWavesSystemGuid ();

	// Guid de l'item du système correspondant à l'identifiant d'article
	// (APINULLGuid si introuvable).
	static API_Guid	FindItemGuid (const API_Guid& systemGuid, const GS::UniString& articleId);

	// Crée le système "CostWaves" s'il est absent, puis un item par article
	// manquant (item.id = article.id, item.name = article.name).
	// Opération globale annulable (une seule entrée d'undo).
	// outCreatedItems = items réellement créés.
	static GSErrCode	EnsureCostWavesClassification (const GS::Array<CWArticle>& articles,
													  API_Guid& outSystemGuid, USize& outCreatedItems,
													  GS::UniString& outError);

	// Crée (si absents) le groupe de propriétés "CostWaves" et la propriété
	// texte CW_Article_ID. Retourne le guid de la définition
	// (APINULLGuid + outError en cas d'échec).
	static API_Guid	EnsureArticleIdProperty (GS::UniString& outError);

	// Affecte l'item aux éléments dans le système donné ; remplace la classe
	// éventuellement déjà portée par chaque élément dans ce système. Si
	// articleIdPropGuid est valide, écrit aussi articleId dans la propriété
	// CW_Article_ID de chaque élément. Tout est regroupé dans UNE seule
	// commande annulable. Les échecs individuels sont comptés (best effort) ;
	// outError n'est rempli que si tout a échoué.
	static GSErrCode	AssignArticleToElements (const GS::Array<API_Guid>&	elemGuids,
												const API_Guid&				systemGuid,
												const API_Guid&				itemGuid,
												const GS::UniString&		articleId,
												const API_Guid&				articleIdPropGuid,
												USize&						outChangedCount,
												USize&						outFailedCount,
												GS::UniString&				outError);

	// Bouton « Créer le matériau… » (phase 5) : crée (ou met à jour) UN matériau
	// de construction avec ses attributs (hachure, surface, stylos, puissance)
	// et le lie à une classe de classification :
	//  - createNewClass : crée l'item classId/className sous parentItemGuid
	//    dans systemGuid (annulable), puis affecte l'item au matériau ;
	//  - sinon : affecte existingItemGuid au matériau.
	// La création d'attributs n'est PAS annulable (limite API) ; tout le reste
	// est regroupé dans UNE commande annulable.
	static GSErrCode	CreateMaterialWithClass (const GS::UniString&	materialName,
										   const CWMaterialAttributes&	attributes,
										   bool							createNewClass,
										   const API_Guid&				systemGuid,
										   const API_Guid&				parentItemGuid,
										   const GS::UniString&			classId,
										   const GS::UniString&			className,
										   const API_Guid&				existingItemGuid,
										   API_Guid&					outItemGuid,
										   bool&						outMaterialCreated,
										   bool&						outClassCreated,
										   GS::UniString&				outError);

	// Crée un item de classification sous un parent donné (APINULLGuid = racine).
	// L'identifiant doit être libre dans tout le système. Retourne le guid de
	// l'item créé (APINULLGuid + outError en cas d'échec).
	static GSErrCode	CreateClassificationItem (const API_Guid& systemGuid, const API_Guid& parentItemGuid,
											const GS::UniString& itemId, const GS::UniString& itemName,
											API_Guid& outItemGuid, GS::UniString& outError);

	// Premier identifiant disponible parmi les enfants du parent (entiers
	// croissants à partir de 1), garanti libre dans tout le système.
	static GS::UniString	FirstAvailableChildId (const API_Guid& systemGuid, const API_Guid& parentItemGuid);

	// Tous les items d'un système (ordre de parcours), avec leur profondeur
	// (0 = racine) pour un affichage indenté. Retourne false si le système est
	// illisible.
	static bool		CollectItems (const API_Guid& systemGuid,
							 GS::Array<API_ClassificationItem>& outItems,
							 GS::Array<short>& outDepths);

	// --- Phase 4 : ensembles CostWaves ---------------------------------------

	// Nom de la propriété texte CW_Group_ID (groupe « CostWaves »).
	static const char*	GroupPropertyName ();

	// Crée (si absents) le groupe « CostWaves » et la propriété texte
	// CW_Group_ID. Retourne le guid de la définition (APINULLGuid + outError).
	static API_Guid		EnsureGroupIdProperty (GS::UniString& outError);

	// Guid de la définition CW_Group_ID si elle existe déjà, sans rien créer
	// (APINULLGuid sinon) — utilisé pour la lecture au scan.
	static API_Guid		FindGroupIdPropertyGuid ();

	// GUID de la définition CW_Article_ID SANS création (repli de classe
	// des dessins 2D — spec §8/§9). APINULLGuid si absente.
	static API_Guid		FindArticleIdPropertyGuid ();

	// Génère un identifiant d'ensemble unique ("CW-E-…") parmi les
	// identifiants déjà utilisés (existingIds).
	static GS::UniString	GenerateGroupId (const GS::Array<GS::UniString>& existingIds);

	// --- Groupes numérotés (phase 5) ------------------------------------------

	// La valeur CW_Group_ID désigne-t-elle un groupe numéroté ("CW-N-<n>") ?
	static bool		IsNumberedGroupValue (const GS::UniString& groupValue);

	// Extrait le numéro d'un groupe numéroté (false si ce n'en est pas un).
	static bool		ParseNumberedGroupValue (const GS::UniString& groupValue, int& outNumber);

	// Valeur CW_Group_ID d'un groupe numéroté ("CW-N-<n>").
	static GS::UniString	NumberedGroupValue (int number);

	// Numéro du prochain groupe numéroté (max des numéros existants + 1).
	static int		NextGroupNumber (const GS::Array<GS::UniString>& existingValues);

	// « Créer un ensemble » / « Créer un groupe » : affecte l'article (classe +
	// CW_Article_ID, comme AssignArticleToElements) et pose groupValue dans
	// CW_Group_ID sur chaque élément — le tout dans UNE seule commande
	// annulable (undoTitle = libellé de la commande d'annulation).
	// Les membres sont « consommés » : ils ne sont plus facturés
	// individuellement, l'ensemble (ou le groupe) l'est à leur place.
	static GSErrCode	CreateGroupFromElements (const GS::Array<API_Guid>&	elemGuids,
											const API_Guid&			systemGuid,
											const API_Guid&			itemGuid,
											const GS::UniString&		articleId,
											const API_Guid&			articleIdPropGuid,
											const GS::UniString&		groupId,
											const API_Guid&			groupIdPropGuid,
											const GS::UniString&		undoTitle,
											USize&					outChangedCount,
											USize&					outFailedCount,
											GS::UniString&			outError);

	// « Dissoudre l'ensemble » : retire CW_Group_ID (valeur vidée) sur chaque
	// élément — une seule commande annulable. La classe et CW_Article_ID sont
	// conservées : l'élément redevient facturable individuellement.
	static GSErrCode	DissolveGroupFromElements (const GS::Array<API_Guid>&	elemGuids,
												  const API_Guid&			groupIdPropGuid,
												  USize&					outChangedCount,
												  USize&					outFailedCount,
												  GS::UniString&			outError);

	// Quantité facturée d'une ligne pour l'article donné :
	//  - groupe numéroté : 1 par groupe (le nombre de groupes est la quantité
	//    réelle du métré)
	//  - unité ENS (ou vide) : forfait, 1 par ligne facturée (élément ou ensemble)
	//  - autre unité : première quantité de la ligne portant cette unité ;
	//    pour une ligne ensemble, somme des quantités de ses membres
	//  - ligne consommée : 0 (facturée via son ensemble/groupe)
	// outUnit reçoit l'unité de facturation effective.
	static double	ComputeBilledQuantity (const CWArticle&			article,
										  const CWElementRow&			row,
										  const GS::Array<CWElementRow>&	allRows,
										  GS::UniString&				outUnit);

	// L'unité est-elle une facturation « à l'ensemble » (forfait) ?
	static bool		IsEnsUnit (const GS::UniString& unit);

	// Première quantité de la liste portant l'unité donnée (0 si absente).
	// Comparaison normalisée (majuscules, ² -> 2, ³ -> 3).
	static double	QuantityForUnit (const GS::Array<CWQuantity>& quantities, const GS::UniString& unit);

	// Récapitulatif par article : parcourt les lignes facturables (éléments
	// libres + ensembles ; les membres consommés sont exclus) et totalise
	// les quantités facturées par article (identifié par la classe).
	// Le mode (§3) détermine la facturation : CWQuantMode::Element -> les
	// éléments (jamais leurs skins) ; CWQuantMode::Component -> les skins
	// classés (jamais l'élément parent). Les dessins 2D sont toujours
	// facturés comme éléments.
	static void	BuildArticleSummary (const GS::Array<CWElementRow>&	rows,
									   const GS::Array<CWArticle>&		articles,
									   CWQuantMode						mode,
									   GS::Array<CWArticleSummary>&	outSummary);

private:
	// Crée (si absents) le groupe « CostWaves » et la propriété texte donnée
	// dans ce groupe ; retourne le guid de la définition.
	static API_Guid		EnsureTextProperty (const char* nameUtf8, const GS::UniString& description,
											  GS::UniString& outError);

	// Enumère tous les items (racines + enfants récursifs) du système.
	static void		EnumerateItems (const API_Guid& systemGuid, GS::Array<API_ClassificationItem>& outItems);
	static void		CollectChildren (const API_Guid& parentGuid, GS::Array<API_ClassificationItem>& outItems);

	// Système "CostWaves" existant ou créé (APINULLGuid + outError si échec).
	static API_Guid	EnsureCostWavesSystem (GS::UniString& outError);

	// Item de l'article dans le système, créé s'il est absent.
	static API_Guid	EnsureArticleItem (const API_Guid& systemGuid, const CWArticle& article,
										bool& outCreated, GS::UniString& outError);
};

} // namespace CostWaves

#endif // COSTWAVES_ARTICLE_MANAGER_HPP
