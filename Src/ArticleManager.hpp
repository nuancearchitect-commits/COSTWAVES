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

	// Bouton « Créer les matériaux » : pour chaque article —
	//  1) matériau de construction "id — nom" (créé s'il n'existe pas déjà :
	//     la création d'attribut est idempotente par nom)
	//  2) item de classification dans le système "CostWaves" (créé si absent)
	//  3) affectation de l'item au matériau (remplace la classe précédente)
	// La création d'attributs n'est PAS annulable (limite API) ; la partie
	// classification est regroupée dans une commande annulable.
	static GSErrCode	CreateBuildingMaterials (const GS::Array<CWArticle>& articles,
												API_Guid& outSystemGuid,
												USize& outCreatedMaterials, USize& outCreatedItems,
												USize& outAssigned,
												GS::UniString& outError);

private:
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
