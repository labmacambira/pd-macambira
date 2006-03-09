/* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.

iem_bin_ambi written by Thomas Musil, Copyright (c) IEM KUG Graz Austria 2000 - 2005 */

#ifdef NT
#pragma warning( disable : 4244 )
#pragma warning( disable : 4305 )
#endif


#include "m_pd.h"
#include "iemlib.h"
#include "iem_bin_ambi.h"
#include <math.h>
#include <stdio.h>
#include <string.h>


/* -------------------------- bin_ambi_reduced_decode_fir2 ------------------------------ */
/*
 ** berechnet ein reduziertes Ambisonic-Decoder-Set in die HRTF-Spektren **
 ** Inputs: ls + Liste von 3 floats: Index [1 .. 16] + Elevation [-90 .. +90 degree] + Azimut [0 .. 360 degree] **
 ** Inputs: calc_inv **
 ** Inputs: load_HRIR + float index1..16 **
 ** Outputs: List of 2 symbols: left-HRIR-File-name + HRIR-table-name **
 ** Inputs: calc_reduced **
 ** "output" ...  writes the HRTF into tables **
 **  **
 **  **
 ** setzt voraus , dass die HRIR-tabele-names von 1016_1_L_HRIR .. 1016_16_L_HRIR heissen und existieren **
 ** setzt voraus , dass die HRTF-tabele-names von 1016_1_HRTF_re .. 1016_16_HRTF_re heissen und existieren **
 ** setzt voraus , dass die HRTF-tabele-names von 1016_1_HRTF_im .. 1016_16_HRTF_im heissen und existieren **
 */


typedef struct _bin_ambi_reduced_decode_fir2
{
	t_object			x_obj;
	t_atom				x_at[2];/*output filename and tablename of HRIR*/
	int						x_n_dim;/*dimension of ambisonic system*/
	int						x_n_ambi;/*number of ambi channels*/
	int						x_n_order;/*order of ambisonic system*/
	int						x_n_real_ls;/*number of real loudspeakers*/
	int						x_n_pht_ls;/*number of phantom loudspeakers*/
	int						x_seq_ok;
	int						x_firsize;/*size of FIR-filter*/
	double				*x_inv_work1;/* n_ambi*n_ambi buffer for matrix inverse */
	double				*x_inv_work2;/* 2*n_ambi*n_ambi buffer for matrix inverse */
	double				*x_inv_buf2;/* 2*n_ambi buffer for matrix inverse */
	double				*x_transp;/* n_ls*n_ambi transposed input-buffer of decoder-matrix before inversion */
	double				*x_ls_encode;/* n_ambi*n_ls straight input-buffer of decoder-matrix before inversion */
	double				*x_prod2;/* n_ls*n_ambi output-buffer of decoder-matrix after inversion */
	double				*x_prod3;/* n_ls*n_ambi output-buffer of decoder-matrix after inversion */
	double				*x_ambi_channel_weight;
	int						*x_delta;
	int						*x_phi;
	int						*x_phi_sym;
	int						*x_sym_flag;
	float					*x_beg_fade_out_hrir;
	float					*x_beg_hrir;
	float					**x_beg_hrir_red;
	t_symbol			**x_hrir_filename;
	t_symbol			**x_s_hrir;
	t_symbol			**x_s_hrir_red;
	t_symbol			*x_s_fade_out_hrir;
	void					*x_out_sign_sum;
	double				x_sqrt3;
	double				x_sqrt10_4;
	double				x_sqrt15_2;
	double				x_sqrt6_4;
	double				x_sqrt35_8;
	double				x_sqrt70_4;
	double				x_sqrt5_2;
	double				x_sqrt126_16;
	double				x_sqrt315_8;
	double				x_sqrt105_4;
	double				x_pi_over_180;
	double				x_sing_range;
} t_bin_ambi_reduced_decode_fir2;

static t_class *bin_ambi_reduced_decode_fir2_class;

static void bin_ambi_reduced_decode_fir2_convert(t_bin_ambi_reduced_decode_fir2 *x, double *delta_deg2rad, double *phi_deg2rad, int index)
{
	double q = 1.0;
	double d = *delta_deg2rad;
	double p = *phi_deg2rad;
	int i;

	if(d < -90.0)
		d = -90.0;
	if(d > 90.0)
		d = 90.0;
	while(p < 0.0)
		p += 360.0;
	while(p >= 360.0)
		p -= 360.0;

	x->x_delta[index] = (int)(d);
	x->x_phi[index] = (int)(p);

	*delta_deg2rad = d*x->x_pi_over_180;
	*phi_deg2rad = p*x->x_pi_over_180;
}

static void bin_ambi_reduced_decode_fir2_do_2d(t_bin_ambi_reduced_decode_fir2 *x, int argc, t_atom *argv, int mode)
{
	double delta=0.0, phi;
	double *dw = x->x_transp;
	int index;
	int order=x->x_n_order;

	if(argc < 2)
	{
		post("bin_ambi_reduced_decode_fir2 ERROR: ls-input needs 1 index and 1 angle: ls_index + phi [degree]");
		return;
	}
	index = (int)atom_getint(argv++) - 1;
	phi = (double)atom_getfloat(argv);

	if(index < 0)
		index = 0;
	if(mode == BIN_AMBI_LS_REAL)
	{
		if(index >= x->x_n_real_ls)
			index = x->x_n_real_ls - 1;
	}
	else if(mode == BIN_AMBI_LS_PHT)
	{
		if(x->x_n_pht_ls)
		{
			if(index >= x->x_n_pht_ls)
				index = x->x_n_pht_ls - 1;
			index += x->x_n_real_ls;
		}
		else
			return;
	}
	else
		return;

	bin_ambi_reduced_decode_fir2_convert(x, &delta, &phi, index);

	dw += index * x->x_n_ambi;

	*dw++ = 1.0;
	*dw++ = cos(phi);
	*dw++ = sin(phi);

	if(order >= 2)
	{
		*dw++ = cos(2.0*phi);
		*dw++ = sin(2.0*phi);

		if(order >= 3)
		{
			*dw++ = cos(3.0*phi);
			*dw++ = sin(3.0*phi);
			if(order >= 4)
			{
				*dw++ = cos(4.0*phi);
				*dw++ = sin(4.0*phi);

				if(order >= 5)
				{
					*dw++ = cos(5.0*phi);
					*dw++ = sin(5.0*phi);

					if(order >= 6)
					{
						*dw++ = cos(6.0*phi);
						*dw++ = sin(6.0*phi);

						if(order >= 7)
						{
							*dw++ = cos(7.0*phi);
							*dw++ = sin(7.0*phi);

							if(order >= 8)
							{
								*dw++ = cos(8.0*phi);
								*dw++ = sin(8.0*phi);

								if(order >= 9)
								{
									*dw++ = cos(9.0*phi);
									*dw++ = sin(9.0*phi);

									if(order >= 10)
									{
										*dw++ = cos(10.0*phi);
										*dw++ = sin(10.0*phi);

										if(order >= 11)
										{
											*dw++ = cos(11.0*phi);
											*dw++ = sin(11.0*phi);

											if(order >= 12)
											{
												*dw++ = cos(12.0*phi);
												*dw++ = sin(12.0*phi);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

static void bin_ambi_reduced_decode_fir2_do_3d(t_bin_ambi_reduced_decode_fir2 *x, int argc, t_atom *argv, int mode)
{
	double delta, phi;
	double cd, sd, cd2, cd3, sd2, csd, cp, sp, cp2, sp2, cp3, sp3, cp4, sp4;
	double *dw = x->x_transp;
	int index;
	int order=x->x_n_order;

	if(argc < 3)
	{
		post("bin_ambi_reduced_decode_fir2 ERROR: ls-input needs 1 index and 2 angles: ls index + delta [degree] + phi [degree]");
		return;
	}
	index = (int)atom_getint(argv++) - 1;
	delta = atom_getfloat(argv++);
	phi = atom_getfloat(argv);

	if(index < 0)
		index = 0;
	if(mode == BIN_AMBI_LS_REAL)
	{
		if(index >= x->x_n_real_ls)
			index = x->x_n_real_ls - 1;
	}
	else if(mode == BIN_AMBI_LS_PHT)
	{
		if(x->x_n_pht_ls)
		{
			if(index >= x->x_n_pht_ls)
				index = x->x_n_pht_ls - 1;
			index += x->x_n_real_ls;
		}
		else
			return;
	}
	else
		return;
	
	bin_ambi_reduced_decode_fir2_convert(x, &delta, &phi, index);

	cd = cos(delta);
	sd = sin(delta);
	cp = cos(phi);
	sp = sin(phi);
	
	dw += index * x->x_n_ambi;

	*dw++ = 1.0;

	*dw++ = cd * cp;
	*dw++ = cd * sp;
	*dw++ = sd;

	if(order >= 2)
	{
		cp2 = cos(2.0*phi);
		sp2 = sin(2.0*phi);
		cd2 = cd * cd;
		sd2 = sd * sd;
		csd = cd * sd;
		*dw++ = 0.5 * x->x_sqrt3 * cd2 * cp2;
		*dw++ = 0.5 * x->x_sqrt3 * cd2 * sp2;
		*dw++ = x->x_sqrt3 * csd * cp;
		*dw++ = x->x_sqrt3 * csd * sp;
		*dw++ = 0.5 * (3.0 * sd2 - 1.0);

		if(order >= 3)
		{
			cp3 = cos(3.0*phi);
			sp3 = sin(3.0*phi);
			cd3 = cd2 * cd;
			*dw++ = x->x_sqrt10_4 * cd3 * cp3;
			*dw++ = x->x_sqrt10_4 * cd3 * sp3;
			*dw++ = x->x_sqrt15_2 * cd * csd * cp2;
			*dw++ = x->x_sqrt15_2 * cd * csd * sp2;
			*dw++ = x->x_sqrt6_4 * cd * (5.0 * sd2 - 1.0) * cp;
			*dw++ = x->x_sqrt6_4 * cd * (5.0 * sd2 - 1.0) * sp;
			*dw++ = 0.5 * sd * (5.0 * sd2 - 3.0);

			if(order >= 4)
			{
				cp4 = cos(4.0*phi);
				sp4 = sin(4.0*phi);
				*dw++ = x->x_sqrt35_8 * cd2 * cd2 * cp4;
				*dw++ = x->x_sqrt35_8 * cd2 * cd2 * sp4;
				*dw++ = x->x_sqrt70_4 * cd2 * csd * cp3;
				*dw++ = x->x_sqrt70_4 * cd2 * csd * sp3;
				*dw++ = 0.5 * x->x_sqrt5_2 * cd2 * (7.0 * sd2 - 1.0) * cp2;
				*dw++ = 0.5 * x->x_sqrt5_2 * cd2 * (7.0 * sd2 - 1.0) * sp2;
				*dw++ = x->x_sqrt10_4 * csd * (7.0 * sd2 - 3.0) * cp;
				*dw++ = x->x_sqrt10_4 * csd * (7.0 * sd2 - 3.0) * sp;
				*dw++ = 0.125 * (sd2 * (35.0 * sd2 - 30.0) + 3.0);

				if(order >= 5)
				{
					*dw++ = x->x_sqrt126_16 * cd3 * cd2 * cos(5.0*phi);
					*dw++ = x->x_sqrt126_16 * cd3 * cd2 * sin(5.0*phi);
					*dw++ = x->x_sqrt315_8 * cd3 * csd * cp4;
					*dw++ = x->x_sqrt315_8 * cd3 * csd * sp4;
					*dw++ = 0.25 * x->x_sqrt70_4 * cd3 * (9.0 * sd2 - 1.0) * cp3;
					*dw++ = 0.25 * x->x_sqrt70_4 * cd3 * (9.0 * sd2 - 1.0) * sp3;
					*dw++ = x->x_sqrt105_4 * cd * csd * (3.0 * sd2 - 1.0) * cp2;
					*dw++ = x->x_sqrt105_4 * cd * csd * (3.0 * sd2 - 1.0) * sp2;
					*dw++ = 0.25 * x->x_sqrt15_2 * cd * (sd2 * (21.0 * sd2 - 14.0) + 1.0) * cp;
					*dw++ = 0.25 * x->x_sqrt15_2 * cd * (sd2 * (21.0 * sd2 - 14.0) + 1.0) * sp;
					*dw = 0.125 * sd * (sd2 * (63.0 * sd2 - 70.0) + 15.0);
				}
			}
		}
	}
}

static void bin_ambi_reduced_decode_fir2_real_ls(t_bin_ambi_reduced_decode_fir2 *x, t_symbol *s, int argc, t_atom *argv)
{
	if(x->x_n_dim == 2)
		bin_ambi_reduced_decode_fir2_do_2d(x, argc, argv, BIN_AMBI_LS_REAL);
	else
		bin_ambi_reduced_decode_fir2_do_3d(x, argc, argv, BIN_AMBI_LS_REAL);
	x->x_seq_ok = 1;
}

static void bin_ambi_reduced_decode_fir2_pht_ls(t_bin_ambi_reduced_decode_fir2 *x, t_symbol *s, int argc, t_atom *argv)
{
	if(x->x_n_dim == 2)
		bin_ambi_reduced_decode_fir2_do_2d(x, argc, argv, BIN_AMBI_LS_PHT);
	else
		bin_ambi_reduced_decode_fir2_do_3d(x, argc, argv, BIN_AMBI_LS_PHT);
}

static void bin_ambi_reduced_decode_fir2_copy_row2buf(t_bin_ambi_reduced_decode_fir2 *x, int row)
{
	int n_ambi2 = 2*x->x_n_ambi;
	int i;
	double *dw=x->x_inv_work2;
	double *db=x->x_inv_buf2;

	dw += row*n_ambi2;
	for(i=0; i<n_ambi2; i++)
		*db++ = *dw++;
}

static void bin_ambi_reduced_decode_fir2_copy_buf2row(t_bin_ambi_reduced_decode_fir2 *x, int row)
{
	int n_ambi2 = 2*x->x_n_ambi;
	int i;
	double *dw=x->x_inv_work2;
	double *db=x->x_inv_buf2;

	dw += row*n_ambi2;
	for(i=0; i<n_ambi2; i++)
		*dw++ = *db++;
}

static void bin_ambi_reduced_decode_fir2_copy_row2row(t_bin_ambi_reduced_decode_fir2 *x, int src_row, int dst_row)
{
	int n_ambi2 = 2*x->x_n_ambi;
	int i;
	double *dw_src=x->x_inv_work2;
	double *dw_dst=x->x_inv_work2;

	dw_src += src_row*n_ambi2;
	dw_dst += dst_row*n_ambi2;
	for(i=0; i<n_ambi2; i++)
		*dw_dst++ = *dw_src++;
}

static void bin_ambi_reduced_decode_fir2_xch_rows(t_bin_ambi_reduced_decode_fir2 *x, int row1, int row2)
{
	bin_ambi_reduced_decode_fir2_copy_row2buf(x, row1);
	bin_ambi_reduced_decode_fir2_copy_row2row(x, row2, row1);
	bin_ambi_reduced_decode_fir2_copy_buf2row(x, row2);
}

static void bin_ambi_reduced_decode_fir2_mul_row(t_bin_ambi_reduced_decode_fir2 *x, int row, double mul)
{
	int n_ambi2 = 2*x->x_n_ambi;
	int i;
	double *dw=x->x_inv_work2;

	dw += row*n_ambi2;
	for(i=0; i<n_ambi2; i++)
	{
		(*dw) *= mul;
		dw++;
	}
}

static void bin_ambi_reduced_decode_fir2_mul_col(t_bin_ambi_reduced_decode_fir2 *x, int col, double mul)
{
	int n_ambi = x->x_n_ambi;
	int n_ambi2 = 2*n_ambi;
	int i;
	double *dw=x->x_inv_work2;

	dw += col;
	for(i=0; i<n_ambi; i++)
	{
		(*dw) *= mul;
		dw += n_ambi2;
	}
}

static void bin_ambi_reduced_decode_fir2_mul_buf_and_add2row(t_bin_ambi_reduced_decode_fir2 *x, int row, double mul)
{
	int n_ambi2 = 2*x->x_n_ambi;
	int i;
	double *dw=x->x_inv_work2;
	double *db=x->x_inv_buf2;

	dw += row*n_ambi2;
	for(i=0; i<n_ambi2; i++)
	{
		*dw += (*db)*mul;
		dw++;
		db++;
	}
}

static int bin_ambi_reduced_decode_fir2_eval_which_element_of_col_not_zero(t_bin_ambi_reduced_decode_fir2 *x, int col, int start_row)
{
	int n_ambi = x->x_n_ambi;
	int n_ambi2 = 2*n_ambi;
	int i, j;
	double *dw=x->x_inv_work2;
	double singrange=x->x_sing_range;
	int ret=-1;

	dw += start_row*n_ambi2 + col;
	j = 0;
	for(i=start_row; i<n_ambi; i++)
	{
		if((*dw > singrange) || (*dw < -singrange))
		{
			ret = i;
			i = n_ambi+1;
		}
		dw += n_ambi2;
	}
	return(ret);
}

static void bin_ambi_reduced_decode_fir2_mul1(t_bin_ambi_reduced_decode_fir2 *x)
{
	double *vec1, *beg1=x->x_ls_encode;
	double *vec2, *beg2=x->x_ls_encode;
	double *inv=x->x_inv_work1;
	double sum;
	int n_ls=x->x_n_real_ls+x->x_n_pht_ls;
	int n_ambi=x->x_n_ambi;
	int i, j, k;

	for(k=0; k<n_ambi; k++)
	{
		beg2=x->x_ls_encode;
		for(j=0; j<n_ambi; j++)
		{
			sum = 0.0;
			vec1 = beg1;
			vec2 = beg2;
			for(i=0; i<n_ls; i++)
			{
				sum += *vec1++ * *vec2++;
			}
			beg2 += n_ls;
			*inv++ = sum;
		}
		beg1 += n_ls;
	}
}

static void bin_ambi_reduced_decode_fir2_mul2(t_bin_ambi_reduced_decode_fir2 *x)
{
	int n_ls=x->x_n_real_ls+x->x_n_pht_ls;
	int n_ambi=x->x_n_ambi;
	int n_ambi2=2*n_ambi;
	int i, j, k;
	double *vec1, *beg1=x->x_transp;
	double *vec2, *beg2=x->x_inv_work2+n_ambi;
	double *vec3=x->x_prod2;
	double *acw_vec=x->x_ambi_channel_weight;
	double sum;

	for(k=0; k<n_ls; k++)
	{
		beg2=x->x_inv_work2+n_ambi;
		for(j=0; j<n_ambi; j++)
		{
			sum = 0.0;
			vec1 = beg1;
			vec2 = beg2;
			for(i=0; i<n_ambi; i++)
			{
				sum += *vec1++ * *vec2;
				vec2 += n_ambi2;
			}
			beg2++;
			*vec3++ = sum * acw_vec[j];
		}
		beg1 += n_ambi;
	}
}

/* wird ersetzt durch haendische spiegelung und reduzierung
static void bin_ambi_reduced_decode_fir2_mul3(t_bin_ambi_reduced_decode_fir2 *x)
{
	int i, n;
	double *dv3=x->x_prod3;
	double *dv2=x->x_prod2;
	double *dv1=x->x_prod2;
	double mw=x->x_mirror_weight;

	n = x->x_n_ind_ls * x->x_n_ambi;
	for(i=0; i<n; i++)
		*dv3++ = *dv1++;
	dv2 += n;
	n = x->x_n_mrg_mir_ls * x->x_n_ambi;
	dv2 += n;
	for(i=0; i<n; i++)
		*dv3++ = *dv1++ + (*dv2++)*mw;
}*/

static void bin_ambi_reduced_decode_fir2_transp_back(t_bin_ambi_reduced_decode_fir2 *x)
{
	double *vec, *transp=x->x_transp;
	double *straight=x->x_ls_encode;
	int n_ls=x->x_n_real_ls+x->x_n_pht_ls;
	int n_ambi=x->x_n_ambi;
	int i, j;

	transp = x->x_transp;
	for(j=0; j<n_ambi; j++)
	{
		vec = transp;
		for(i=0; i<n_ls; i++)
		{
			*straight++ = *vec;
			vec += n_ambi;
		}
		transp++;
	}
}

static void bin_ambi_reduced_decode_fir2_inverse(t_bin_ambi_reduced_decode_fir2 *x)
{
	int n_ambi = x->x_n_ambi;
	int n_ambi2 = 2*n_ambi;
	int i, j, nz;
	int r,c;
	double *src=x->x_inv_work1;
	double *db=x->x_inv_work2;
	double rcp, *dv;

	dv = db;
	for(i=0; i<n_ambi; i++) /* init */
	{
		for(j=0; j<n_ambi; j++)
		{
			*dv++ = *src++;
		}
		for(j=0; j<n_ambi; j++)
		{
			if(j == i)
				*dv++ = 1.0;
			else
				*dv++ = 0.0;
		}
	}

		/* make 1 in main-diagonale, and 0 below */
	for(i=0; i<n_ambi; i++)
	{
		nz = bin_ambi_reduced_decode_fir2_eval_which_element_of_col_not_zero(x, i, i);
		if(nz < 0)
		{
			post("bin_ambi_reduced_decode_fir2 ERROR: matrix not regular !!!!");
			x->x_seq_ok = 0;
			return;
		}
		else
		{
			if(nz != i)
				bin_ambi_reduced_decode_fir2_xch_rows(x, i, nz);
			dv = db + i*n_ambi2 + i;
			rcp = 1.0 /(*dv);
			bin_ambi_reduced_decode_fir2_mul_row(x, i, rcp);
			bin_ambi_reduced_decode_fir2_copy_row2buf(x, i);
			for(j=i+1; j<n_ambi; j++)
			{
				dv += n_ambi2;
				rcp = -(*dv);
				bin_ambi_reduced_decode_fir2_mul_buf_and_add2row(x, j, rcp);
			}
		}
	}

			/* make 0 above the main diagonale */
	for(i=n_ambi-1; i>=0; i--)
	{
		dv = db + i*n_ambi2 + i;
		bin_ambi_reduced_decode_fir2_copy_row2buf(x, i);
		for(j=i-1; j>=0; j--)
		{
			dv -= n_ambi2;
			rcp = -(*dv);
			bin_ambi_reduced_decode_fir2_mul_buf_and_add2row(x, j, rcp);
		}
	}

	post("matrix_inverse regular");
	x->x_seq_ok = 1;
}

static void bin_ambi_reduced_decode_fir2_calc_pinv(t_bin_ambi_reduced_decode_fir2 *x)
{
	t_garray *a;
	int npoints;
	t_float *fadevec;
	int i, n;
	double *dv3=x->x_prod3;
	double *dv2=x->x_prod2;

	if((int)(x->x_beg_fade_out_hrir) == 0)
	{
		if (!(a = (t_garray *)pd_findbyclass(x->x_s_fade_out_hrir, garray_class)))
			error("%s: no such array", x->x_s_fade_out_hrir->s_name);
		else if (!garray_getfloatarray(a, &npoints, &fadevec))
			error("%s: bad template for bin_ambi_reduced_decode_fir2", x->x_s_fade_out_hrir->s_name);
		else if (npoints < x->x_firsize)
			error("%s: bad array-size: %d", x->x_s_fade_out_hrir->s_name, npoints);
		else
			x->x_beg_fade_out_hrir = fadevec;
	}

	bin_ambi_reduced_decode_fir2_transp_back(x);
	bin_ambi_reduced_decode_fir2_mul1(x);
	bin_ambi_reduced_decode_fir2_inverse(x);
	bin_ambi_reduced_decode_fir2_mul2(x);
	
	n = x->x_n_real_ls * x->x_n_ambi;
	for(i=0; i<n; i++)
		*dv3++ = *dv2++;
}

/*
x_prod:
n_ambi  columns;
n_ambi    rows;
*/

static void bin_ambi_reduced_decode_fir2_ipht_ireal_muladd(t_bin_ambi_reduced_decode_fir2 *x, t_symbol *s, int argc, t_atom *argv)
{
	int n = x->x_n_ambi;
	int i, pht_index, real_index;
	double *dv3=x->x_prod3;
	double *dv2=x->x_prod2;
	float mw;

	if(argc < 3)
	{
		post("bin_ambi_reduced_decode_fir2 ERROR: ipht_ireal_muladd needs 2 index and 1 mirrorweight: pht_ls_index + real_ls_index + mirror_weight_element");
		return;
	}
	pht_index = (int)atom_getint(argv++) - 1;
	real_index = (int)atom_getint(argv++) - 1;
	mw = (double)atom_getfloat(argv);

	if(pht_index < 0)
		pht_index = 0;
	if(real_index < 0)
		real_index = 0;
	if(real_index >= x->x_n_real_ls)
		real_index = x->x_n_real_ls - 1;
	if(pht_index >= x->x_n_pht_ls)
		pht_index = x->x_n_pht_ls - 1;

	dv3 += real_index*x->x_n_ambi;
	dv2 += (x->x_n_real_ls+pht_index)*x->x_n_ambi;
	for(i=0; i<n; i++)
	{
		*dv3 += (*dv2++)*mw;
		dv3++;
	}
}

static void bin_ambi_reduced_decode_fir2_load_HRIR(t_bin_ambi_reduced_decode_fir2 *x, t_symbol *s, int argc, t_atom *argv)
{
	int index;
	t_symbol *hrirname;

	if(argc < 2)
	{
		post("bin_ambi_reduced_decode_fir2 ERROR: load_HRIR needs 1 index and 1 HRIR-wav");
		return;
	}
	index = (int)atom_getint(argv++) - 1;
	hrirname = atom_getsymbol(argv);
	if(index < 0)
		index = 0;
	if(index >= x->x_n_real_ls)
		index = x->x_n_real_ls - 1;
	x->x_hrir_filename[index] = hrirname;

	SETSYMBOL(x->x_at, x->x_hrir_filename[index]);
	SETSYMBOL(x->x_at+1, x->x_s_hrir[index]);
	outlet_list(x->x_obj.ob_outlet, &s_list, 2, x->x_at);
}

static void bin_ambi_reduced_decode_fir2_check_HRIR_arrays(t_bin_ambi_reduced_decode_fir2 *x, float findex)
{
	int index=(int)findex - 1;
	int j, k, n;
	int firsize = x->x_firsize;
	t_garray *a;
	int npoints;
	t_symbol *hrir;
	t_float *vec_hrir, *vec, *vec_fade_out_hrir;
	float decr, sum;

	if(index < 0)
		index = 0;
	if(index >= x->x_n_real_ls)
		index = x->x_n_real_ls - 1;

	hrir = x->x_s_hrir[index];
	if (!(a = (t_garray *)pd_findbyclass(hrir, garray_class)))
		error("%s: no such array", hrir->s_name);
	else if (!garray_getfloatarray(a, &npoints, &vec_hrir))
		error("%s: bad template for bin_ambi_reduced_decode_fir2", hrir->s_name);
	else
	{
		if(npoints < firsize)
		{
			post("bin_ambi_reduced_decode_fir2-WARNING: %s-array-size: %d < FIR-size: %d", hrir->s_name, npoints, firsize);
		}
		vec = x->x_beg_hrir;
		vec += index * firsize;
	
		if((int)(x->x_beg_fade_out_hrir))
		{
			vec_fade_out_hrir = x->x_beg_fade_out_hrir;
			for(j=0; j<firsize; j++)
				vec[j] = vec_hrir[j]*vec_fade_out_hrir[j];
		}
		else
		{
			post("no HRIR-fade-out-window found");
			n = firsize * 3;
			n /= 4;
			for(j=0; j<n; j++)
				vec[j] = vec_hrir[j];
			sum = 1.0f;
			decr = 4.0f / (float)firsize;
			for(j=n, k=0; j<firsize; j++, k++)
			{
				sum -= decr;
				vec[j] = vec_hrir[j] * sum;
			}
		}
	}
}

static void bin_ambi_reduced_decode_fir2_check_HRIR_RED_arrays(t_bin_ambi_reduced_decode_fir2 *x, float findex)
{
	int index=(int)findex - 1;
	t_garray *a;
	int npoints;
	int firsize = x->x_firsize;
	t_float *vec_hrir_red;
	t_symbol *hrir_red;

	if(index < 0)
		index = 0;
	if(index >= x->x_n_ambi)
		index = x->x_n_ambi - 1;

	hrir_red = x->x_s_hrir_red[index];

	if (!(a = (t_garray *)pd_findbyclass(hrir_red, garray_class)))
		error("%s: no such array", hrir_red->s_name);
	else if (!garray_getfloatarray(a, &npoints, &vec_hrir_red))
		error("%s: bad template for bin_ambi_reduced_decode_fir2", hrir_red->s_name);
	else if (npoints < firsize)
		error("%s: bad array-size: %d", hrir_red->s_name, npoints);
	else
	{
		x->x_beg_hrir_red[index] = vec_hrir_red;
	}
}

static void bin_ambi_reduced_decode_fir2_calc_reduced(t_bin_ambi_reduced_decode_fir2 *x, float findex)
{
	int index=(int)findex - 1;
	int i, j;
	int firsize = x->x_firsize;
	t_float *vec_hrir, *vec_hrir_red;
	double *dv;
	int n_ambi = x->x_n_ambi;
	int n_ls = x->x_n_real_ls;
	float mul;

	if(x->x_seq_ok)
	{
		if(index < 0)
			index = 0;
		if(index >= n_ambi)
			index = n_ambi - 1;

		vec_hrir_red = x->x_beg_hrir_red[index];

		dv = x->x_prod3 + index;
		mul = (float)(*dv);
		vec_hrir = x->x_beg_hrir;
		for(i=0; i<firsize; i++)/*first step of acumulating the HRIRs*/
		{
			vec_hrir_red[i] = mul * vec_hrir[i];
		}

		for(j=1; j<n_ls; j++)
		{
			dv += n_ambi;
			mul = (float)(*dv);
			vec_hrir = x->x_beg_hrir;
			vec_hrir += j * firsize;
			for(i=0; i<firsize; i++)
			{
				vec_hrir_red[i] += mul * vec_hrir[i];
			}
		}
	}
}

static void bin_ambi_reduced_decode_fir2_calc_sym(t_bin_ambi_reduced_decode_fir2 *x)
{
	int *sym=x->x_phi_sym;
	int n_ls = x->x_n_real_ls;
	int n_ambi = x->x_n_ambi;
	int n_ambi2 = 2*n_ambi;
	int *phi=x->x_phi;
	int *delta=x->x_delta;
	int *flag=x->x_sym_flag;
	int i, j, d, p, regular=1;
	double *dv, *db=x->x_prod3;
	double a, b, q;
	int sym_max=0;
	int pos_sym_counter, neg_sym_counter;
	char plus_minus[100];
	char abc[100];
	char onenine[100];
	char ten[100];

	if(x->x_seq_ok)
	{
		for(i=0; i<n_ls; i++)
			sym[i] = -1;

		plus_minus[0] = 0;
		if(x->x_n_dim == 2)
		{
			strcpy(abc,     "                      WXYXYXYXYXYXYXYXYXYXYXYXYXYXYXYXYXYXYXY");
			strcpy(ten,     "                      000000000000000000011111111111111111111");
			strcpy(onenine, "                      011223344556677889900112233445566778899");
			abc[n_ambi+22] = 0;
			ten[n_ambi+22] = 0;
			onenine[n_ambi+22] = 0;
			post(abc);
			post(ten);
			post(onenine);
		}
		else
		{
			strcpy(abc,     "                      AABCABCDEABCDEFGABCDEFGHIABCDEFGHIJKABCDEFGHIJKLM");
			strcpy(onenine, "                      0111222223333333444444444555555555556666666666666");
			abc[n_ambi+22] = 0;
			onenine[n_ambi+22] = 0;
			post(abc);
			post(onenine);
		}

		for(i=0; i<n_ls; i++)/* looking for azimuth symmetries of loudspeakers */
		{
			d = delta[i];
			p = phi[i];
			for(j=i+1; j<n_ls; j++)
			{
				if(d == delta[j])
				{
					if((p + phi[j]) == 360)
					{
						if((sym[i] < 0) && (sym[j] < 0))
						{
							sym[i] = j;
							sym[j] = i;
							j = n_ls + 1;
							sym_max++;
						}
					}
				}
			}
		}

		for(p=0; p<n_ambi; p++)/*each col*/
		{
			pos_sym_counter = 0;
			neg_sym_counter = 0;
			for(i=0; i<n_ls; i++)
				flag[i] = 1;
			dv = db + p;
			for(i=0; i<n_ls; i++)
			{
				if((sym[i] >= 0) && flag[i])
				{
					j = sym[i];
					flag[i] = 0;
					flag[j] = 0;
					a = dv[i*n_ambi];
					b = dv[j*n_ambi];
					if((a < 5.0e-4)&&(a > -5.0e-4)&&(b < 5.0e-4)&&(b > -5.0e-4))
					{
						pos_sym_counter++;
						neg_sym_counter++;
					}
					else
					{
						q = a / b;
						if((q < 1.005)&&(q > 0.995))
							pos_sym_counter++;
						else if((q > -1.005)&&(q < -0.995))
							neg_sym_counter++;
					}
				}
			}
			if(pos_sym_counter == sym_max)
				strcat(plus_minus, "+");
			else if(neg_sym_counter == sym_max)
				strcat(plus_minus, "-");
			else
			{
				strcat(plus_minus, "?");
				regular = 0;
			}
		}
		post("sum of right channel: %s", plus_minus);

		if(regular)
		{
			for(i=0; i<n_ambi; i++)
			{
				/*change*/
				if(plus_minus[i] == '+')
					SETFLOAT(x->x_at, 1.0f);
				else if(plus_minus[i] == '-')
					SETFLOAT(x->x_at, 2.0f);
				SETFLOAT(x->x_at+1, (float)(i+1));
				outlet_list(x->x_out_sign_sum, &s_list, 2, x->x_at);
			}
		}
	}
}

static void bin_ambi_reduced_decode_fir2_ambi_weight(t_bin_ambi_reduced_decode_fir2 *x, t_symbol *s, int argc, t_atom *argv)
{
	if(argc > x->x_n_order)
	{
		int i, k=0, n=x->x_n_order;
		double d;

		x->x_ambi_channel_weight[k] = (double)atom_getfloat(argv++);
		k++;
		if(x->x_n_dim == 2)
		{
			for(i=1; i<=n; i++)
			{
				d = (double)atom_getfloat(argv++);
				x->x_ambi_channel_weight[k] = d;
				k++;
				x->x_ambi_channel_weight[k] = d;
				k++;
			}
		}
		else
		{
			int j, m;

			for(i=1; i<=n; i++)
			{
				d = (double)atom_getfloat(argv++);
				m = 2*i + 1;
				for(j=0; j<m; j++)
				{
					x->x_ambi_channel_weight[k] = d;
					k++;
				}
			}
		}
	}
	else
		post("bin_ambi_reduced_decode_fir2-ERROR: ambi_weight needs %d float weights", x->x_n_order+1);
}

static void bin_ambi_reduced_decode_fir2_sing_range(t_bin_ambi_reduced_decode_fir2 *x, t_floatarg f)
{
	if(f < 0.0f)
		x->x_sing_range = -(double)f;
	else
		x->x_sing_range = (double)f;
}

static void bin_ambi_reduced_decode_fir2_free(t_bin_ambi_reduced_decode_fir2 *x)
{
	freebytes(x->x_hrir_filename, x->x_n_real_ls * sizeof(t_symbol *));
	freebytes(x->x_s_hrir, x->x_n_real_ls * sizeof(t_symbol *));
	freebytes(x->x_s_hrir_red, x->x_n_ambi * sizeof(t_symbol *));
	freebytes(x->x_inv_work1, x->x_n_ambi * x->x_n_ambi * sizeof(double));
	freebytes(x->x_inv_work2, 2 * x->x_n_ambi * x->x_n_ambi * sizeof(double));
	freebytes(x->x_inv_buf2, 2 * x->x_n_ambi * sizeof(double));
	freebytes(x->x_transp, (x->x_n_real_ls+x->x_n_pht_ls) * x->x_n_ambi * sizeof(double));
	freebytes(x->x_ls_encode, (x->x_n_real_ls+x->x_n_pht_ls) * x->x_n_ambi * sizeof(double));
	freebytes(x->x_prod2, (x->x_n_real_ls+x->x_n_pht_ls) * x->x_n_ambi * sizeof(double));
	freebytes(x->x_prod3, x->x_n_real_ls * x->x_n_ambi * sizeof(double));
	freebytes(x->x_ambi_channel_weight, x->x_n_ambi * sizeof(double));

	freebytes(x->x_delta, (x->x_n_real_ls+x->x_n_pht_ls)* sizeof(int));
	freebytes(x->x_phi, (x->x_n_real_ls+x->x_n_pht_ls) * sizeof(int));
	freebytes(x->x_phi_sym, x->x_n_real_ls * sizeof(int));
	freebytes(x->x_sym_flag, x->x_n_real_ls * sizeof(int));

	freebytes(x->x_beg_hrir, x->x_firsize * x->x_n_real_ls * sizeof(float));
	freebytes(x->x_beg_hrir_red, x->x_n_ambi * sizeof(float *));
}

/*
  1.arg_ int prefix;
	2.arg: t_symbol *hrir_name;
	3.arg: t_symbol *hrir_red_name;
	4.arg: t_symbol *hrtf_im_name;
	5.arg: t_symbol *hrir_fade_out_name;
	6.arg: int ambi_order;
	7.arg: int dim;
	8.arg: int number_of_loudspeakers;
	9.arg: int firsize;
	*/

static void *bin_ambi_reduced_decode_fir2_new(t_symbol *s, int argc, t_atom *argv)
{
	t_bin_ambi_reduced_decode_fir2 *x = (t_bin_ambi_reduced_decode_fir2 *)pd_new(bin_ambi_reduced_decode_fir2_class);
	char buf[400];
	int i, j, ok=0;
	int n_order=0, n_dim=0, n_real_ls=0, n_pht_ls=0, n_ambi=0, firsize=0, prefix=0;
	t_symbol	*s_hrir=gensym("L_HRIR");
	t_symbol	*s_hrir_red=gensym("HRIR_red");
  t_symbol  *s_fade_out_hrir=gensym("HRIR_win");

	if((argc >= 9) &&
		IS_A_FLOAT(argv,0) &&
    IS_A_SYMBOL(argv,1) &&
		IS_A_SYMBOL(argv,2) &&
		IS_A_SYMBOL(argv,3) &&
		IS_A_FLOAT(argv,4) &&
		IS_A_FLOAT(argv,5) &&
		IS_A_FLOAT(argv,6) &&
		IS_A_FLOAT(argv,7) &&
		IS_A_FLOAT(argv,8))
	{
		prefix	= (int)atom_getintarg(0, argc, argv);

		s_hrir								= (t_symbol *)atom_getsymbolarg(1, argc, argv);
		s_hrir_red						= (t_symbol *)atom_getsymbolarg(2, argc, argv);
		s_fade_out_hrir	      = (t_symbol *)atom_getsymbolarg(3, argc, argv);

		n_order		= (int)atom_getintarg(4, argc, argv);
		n_dim			= (int)atom_getintarg(5, argc, argv);
		n_real_ls	= (int)atom_getintarg(6, argc, argv);
		n_pht_ls	= (int)atom_getintarg(7, argc, argv);
		firsize		= (int)atom_getintarg(8, argc, argv);

		ok = 1;
	}
	else if((argc >= 9) &&
		IS_A_FLOAT(argv,0) &&
    IS_A_FLOAT(argv,1) &&
		IS_A_FLOAT(argv,2) &&
		IS_A_FLOAT(argv,3) &&
		IS_A_FLOAT(argv,4) &&
		IS_A_FLOAT(argv,5) &&
		IS_A_FLOAT(argv,6) &&
		IS_A_FLOAT(argv,7) &&
		IS_A_FLOAT(argv,8))
	{
		prefix	= (int)atom_getintarg(0, argc, argv);

		s_hrir								= gensym("L_HRIR");
		s_hrir_red						= gensym("HRIR_red");
		s_fade_out_hrir	      = gensym("HRIR_win");

		n_order		= (int)atom_getintarg(4, argc, argv);
		n_dim			= (int)atom_getintarg(5, argc, argv);
		n_real_ls	= (int)atom_getintarg(6, argc, argv);
		n_pht_ls	= (int)atom_getintarg(7, argc, argv);
		firsize		= (int)atom_getintarg(8, argc, argv);

		ok = 1;
	}

	if(ok)
	{
		if(n_order < 1)
			n_order = 1;

		if(n_dim == 3)
		{
			if(n_order > 5)
				n_order = 5;
			n_ambi = (n_order + 1)*(n_order + 1);
		}
		else
		{
			if(n_order > 12)
				n_order = 12;
			n_dim = 2;
			n_ambi = 2 * n_order + 1;
		}

		if(n_real_ls < 1)
			n_real_ls = 1;
		if(n_pht_ls < 0)
			n_pht_ls = 0;

		if((n_real_ls+n_pht_ls) < n_ambi)
		{
			post("bin_ambi_reduced_decode_fir2-WARNING: Number of all Loudspeakers < Number of Ambisonic-Channels !!!!");
		}

		if(firsize < 32)
			firsize = 32;

		x->x_n_dim			= n_dim;
		x->x_n_ambi			= n_ambi;
		x->x_n_real_ls	= n_real_ls;
		x->x_n_pht_ls		= n_pht_ls;
		x->x_n_order		= n_order;
		x->x_firsize		= firsize;

		x->x_hrir_filename	= (t_symbol **)getbytes(x->x_n_real_ls * sizeof(t_symbol *));
		x->x_s_hrir					= (t_symbol **)getbytes(x->x_n_real_ls * sizeof(t_symbol *));
		x->x_s_hrir_red			= (t_symbol **)getbytes(x->x_n_ambi * sizeof(t_symbol *));
		
		j = x->x_n_real_ls;
		for(i=0; i<j; i++)
		{
			sprintf(buf, "%d_%d_%s", prefix, i+1, s_hrir->s_name);
			x->x_s_hrir[i] = gensym(buf);
		}

		for(i=0; i<n_ambi; i++)
		{
			sprintf(buf, "%d_%d_%s", prefix, i+1, s_hrir_red->s_name);
			x->x_s_hrir_red[i] = gensym(buf);
		}

    sprintf(buf, "%d_%s", prefix, s_fade_out_hrir->s_name);
    x->x_s_fade_out_hrir = gensym(buf);

		x->x_inv_work1	= (double *)getbytes(x->x_n_ambi * x->x_n_ambi * sizeof(double));
		x->x_inv_work2	= (double *)getbytes(2 * x->x_n_ambi * x->x_n_ambi * sizeof(double));
		x->x_inv_buf2		= (double *)getbytes(2 * x->x_n_ambi * sizeof(double));
		x->x_transp			= (double *)getbytes((x->x_n_real_ls+x->x_n_pht_ls) * x->x_n_ambi * sizeof(double));
		x->x_ls_encode	= (double *)getbytes((x->x_n_real_ls+x->x_n_pht_ls) * x->x_n_ambi * sizeof(double));
		x->x_prod2			= (double *)getbytes((x->x_n_real_ls+x->x_n_pht_ls) * x->x_n_ambi * sizeof(double));
		x->x_prod3			= (double *)getbytes(x->x_n_real_ls * x->x_n_ambi * sizeof(double));
		x->x_ambi_channel_weight = (double *)getbytes(x->x_n_ambi * sizeof(double));

		x->x_delta			= (int *)getbytes((x->x_n_real_ls+x->x_n_pht_ls) * sizeof(int));
		x->x_phi				= (int *)getbytes((x->x_n_real_ls+x->x_n_pht_ls) * sizeof(int));
		x->x_phi_sym		= (int *)getbytes(x->x_n_real_ls * sizeof(int));
		x->x_sym_flag		= (int *)getbytes(x->x_n_real_ls * sizeof(int));

		x->x_beg_fade_out_hrir	= (float *)0;
		x->x_beg_hrir						= (float *)getbytes(x->x_firsize * x->x_n_real_ls * sizeof(float));
		x->x_beg_hrir_red				= (float **)getbytes(x->x_n_ambi * sizeof(float *));

		x->x_sqrt3				= sqrt(3.0);
		x->x_sqrt5_2			= sqrt(5.0) / 2.0;
		x->x_sqrt6_4			= sqrt(6.0) / 4.0;
		x->x_sqrt10_4			= sqrt(10.0) / 4.0;
		x->x_sqrt15_2			= sqrt(15.0) / 2.0;
		x->x_sqrt35_8			= sqrt(35.0) / 8.0;
		x->x_sqrt70_4			= sqrt(70.0) / 4.0;
		x->x_sqrt126_16		= sqrt(126.0) / 16.0;
		x->x_sqrt315_8		= sqrt(315.0) / 8.0;
		x->x_sqrt105_4		= sqrt(105.0) / 4.0;
		x->x_pi_over_180	= 4.0 * atan(1.0) / 180.0;
		x->x_sing_range = 1.0e-10;
		x->x_seq_ok = 1;
		for(i=0; i<n_ambi; i++)
			x->x_ambi_channel_weight[i] = 1.0;

		outlet_new(&x->x_obj, &s_list);
		x->x_out_sign_sum = outlet_new(&x->x_obj, &s_list);
		return(x);
	}
	else
	{
		post("bin_ambi_reduced_decode_fir2-ERROR: need 1 float + 3 symbols + 5 floats arguments:");
		post("  prefix(unique-number) + hrir_loudspeaker_name + hrir_redused_name + hrir_fade_out_name +");
		post("   + ambi_order + ambi_dimension + number_of_real_loudspeakers + ");
		post("   + number_of_phantom_loudspeakers + firsize");
		return(0);
	}
}

void bin_ambi_reduced_decode_fir2_setup(void)
{
	bin_ambi_reduced_decode_fir2_class = class_new(gensym("bin_ambi_reduced_decode_fir2"), (t_newmethod)bin_ambi_reduced_decode_fir2_new, (t_method)bin_ambi_reduced_decode_fir2_free,
					 sizeof(t_bin_ambi_reduced_decode_fir2), 0, A_GIMME, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_real_ls, gensym("real_ls"), A_GIMME, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_pht_ls, gensym("pht_ls"), A_GIMME, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_calc_pinv, gensym("calc_pinv"), 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_ipht_ireal_muladd, gensym("ipht_ireal_muladd"), A_GIMME, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_load_HRIR, gensym("load_HRIR"), A_GIMME, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_check_HRIR_arrays, gensym("check_HRIR_arrays"), A_FLOAT, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_check_HRIR_RED_arrays, gensym("check_HRIR_RED_arrays"), A_FLOAT, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_calc_reduced, gensym("calc_reduced"), A_FLOAT, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_calc_sym, gensym("calc_sym"), 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_ambi_weight, gensym("ambi_weight"), A_GIMME, 0);
	class_addmethod(bin_ambi_reduced_decode_fir2_class, (t_method)bin_ambi_reduced_decode_fir2_sing_range, gensym("sing_range"), A_DEFFLOAT, 0);
	class_sethelpsymbol(bin_ambi_reduced_decode_fir2_class, gensym("iemhelp2/help-bin_ambi_reduced_decode_fir2"));
}
/*
Reihenfolge:
n_ls x bin_ambi_reduced_decode_fir2_ls

bin_ambi_reduced_decode_fir2_calc_pinv

n_ls x bin_ambi_reduced_decode_fir2_load_HRIR

n_ls x bin_ambi_reduced_decode_fir2_check_HRIR_arrays

n_ambi x bin_ambi_reduced_decode_fir2_check_HRTF_arrays

n_ambi x bin_ambi_reduced_decode_fir2_calc_reduced

bin_ambi_reduced_decode_fir2_calc_sym

*/
