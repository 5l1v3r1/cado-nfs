#ifndef RECOVERY_H_
#define RECOVERY_H_

#ifdef	__cplusplus
extern "C" {
#endif

#define	STAMPSIZE	(bw_filesize*sizeof(mp_limb_t)*periodicity)

/* �a, �a va certainement finir dans le .h */

struct vs_data {
	struct certificate    * rc;
	int			rev_a_s;
	int			rev_v;
	int			vecnum;
	FILE		     ** a_save;
	int			cstart;	/* redundant wirh rc->r0 when
					   the latter exists */
	int			ds_start;	/* I guess it's also redundant*/
	long			trace_offset;
	struct pinfo	      * tenant;
	bw_vector_block		vec;
	bw_vector_block		sum;
	struct mksol_info_block * msi;
};

	
/* Points d'entr�e. Toutes les fonctions peuvent �tre appel�es en
 * contexte MT, quitte � rendre la main imm�diatement pour n-1 threads.
 */

int	vs_try_startup_sync(struct vs_data **, int, int);
/* Fait l'inventaire des fichiers pertinents qui sont pr�sents.
 * Jette � la moindre incoh�rence ; leur r�solution est � la charge de
 * l'utilisateur.
 * Dit quelle r�vision peut �tre atteinte, au vu des fichiers pr�sents.
 * Remplit la structure struct vs_data avec ces infos.
 *
 * R�vision 0 = on part from scratch.
 *
 * Fait aussi l'inventaire des certificats pr�sents, et cherche un
 * �ventuel fichier t�moin.
 *
 * RETURN: La r�vision.
 */

int	vs_do_startup_sync(struct vs_data *);
/* Fait la mise � jour comme demand�.
 * Reconstruit un certificat jusqu'au stamp derni�rement atteint.
 *
 * RETURN: 0, -1 en cas d'erreur, +errno.
 */

int	vs_checkpoint(struct vs_data *, int);
/* Passe d'un stamp � l'autre.
 * Renomme les fichiers.
 * Change de certificat courant s'il est temps d'en mettre un autre.
 */

/* Les clients ont aussi le droit d'�crire dans a_file */

#ifdef	__cplusplus
}
#endif

#endif	/* RECOVERY_H_ */
