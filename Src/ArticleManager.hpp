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
class ArticleManager {
public:
	// Nom du système de classification créé par l'add-on.
	static const char* CostWavesSystemName ();

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

	// Affecte l'item à l'élément dans le système donné ; remplace la classe
	// éventuellement déjà portée par l'élément dans ce système.
	// Opération annulable. outChanged = true si l'élément a été modifié.
	static GSErrCode	AssignArticleToElement (const API_Guid& elemGuid, const API_Guid& systemGuid,
												const API_Guid& itemGuid, bool& outChanged,
												GS::UniString& outError);

private:
	// Enumère tous les items (racines + enfants récursifs) du système.
	static void		EnumerateItems (const API_Guid& systemGuid, GS::Array<API_ClassificationItem>& outItems);
	static void		CollectChildren (const API_Guid& parentGuid, GS::Array<API_ClassificationItem>& outItems);
};

} // namespace CostWaves

#endif // COSTWAVES_ARTICLE_MANAGER_HPP
