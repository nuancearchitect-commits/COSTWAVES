#ifndef COSTWAVES_COSTWAVES_API_HPP
#define COSTWAVES_COSTWAVES_API_HPP

#include "ACAPinc.h"

#include "DataTypes.hpp"

namespace CostWaves {

// Réglages de communication avec le serveur CostWaves (spéc. §13) —
// persistés dans « CostWaves-settings.json » à côté du PLN.
struct CWApiSettings {
	GS::UniString	endpointUrl;			// URL complète de l'API d'import (https://…/archicad/import)
	GS::UniString	apiKey;					// clé / jeton envoyé en « Authorization: Bearer … »
	GS::UniString	unknownArticleMode;		// articles inconnus du serveur : "project_only" (défaut),
											// "base_and_project" ou "ignore" (spéc. §6)

	bool IsUnknownProjectOnly () const    { return unknownArticleMode != GS::UniString ("base_and_project")
												   && unknownArticleMode != GS::UniString ("ignore"); }
	bool IsUnknownBaseAndProject () const { return unknownArticleMode == GS::UniString ("base_and_project"); }
	bool IsUnknownIgnore () const         { return unknownArticleMode == GS::UniString ("ignore"); }
};

// Résultat d'un envoi vers CostWaves.
struct CWApiSendResult {
	GSErrCode		err = NoError;			// NoError = requête menée à terme (même si le serveur répond 4xx/5xx)
	GS::UniString	error;					// message d'erreur lisible (réseau, TLS, protocole…)
	int				httpStatus = 0;			// code HTTP de la réponse (0 si aucun)
	GS::UniString	rawResponse;			// corps de la réponse (texte)

	// Données comprises dans la réponse JSON du serveur (si présentes).
	bool			hasServerData = false;
	USize			createdLines = 0;		// lignes de métré créées côté CostWaves
	USize			updatedLines = 0;		// lignes mises à jour
	GS::Array<GS::UniString>	unknownArticles;	// articles signalés inconnus (spéc. §6)
	GS::UniString	message;				// message éventuel du serveur
};

// Module « CostWaves API » de l'architecture (spéc. §14) : construction du
// payload (§13), envoi HTTP(S), réglages persistants.
class CostWavesApi
{
public:
	// Réglages : chargement/enregistrement dans CostWaves-settings.json
	// (dossier du PLN, repli Documents). Un chargement sans fichier
	// existant retourne les réglages par défaut (sans erreur).
	static bool	LoadSettings (CWApiSettings& outSettings, GS::UniString& outError);
	static bool	SaveSettings (const CWApiSettings& settings, GS::UniString& outError);

	// Corps de la requête (spéc. §13) : projet + catalogue d'articles +
	// récapitulatif facturé + éléments (éléments classés, ensembles, groupes
	// numérotés) avec, pour chacun, sa classe, son article, sa quantité
	// facturée, toutes ses quantités et ses skins classés (§4).
	static GS::UniString	BuildPayload (const GS::UniString& projectId, const GS::UniString& projectName,
										 const GS::Array<CWElementRow>& rows,
										 const GS::Array<CWArticle>& articles,
										 CWQuantMode mode,
										 const GS::Array<CWArticleSummary>& summary,
										 const CWApiSettings& settings);

	// POST du payload vers settings.endpointUrl (WinHTTP, Windows seul).
	// Appel bloquant pendant l'envoi (délais max 10/30 s).
	static CWApiSendResult	Send (const CWApiSettings& settings, const GS::UniString& payload);

	// Clé JSON normalisée d'un libellé de quantité (« Surface projetée » →
	// « projectedSurface », « Épaisseur » → « thickness »…).
	static GS::UniString	QuantityKey (const GS::UniString& label);
};

} // namespace CostWaves

#endif // COSTWAVES_COSTWAVES_API_HPP
