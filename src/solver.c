#include"SimpleMOC_header.h"

// run one full transport sweep, return k
double transport_sweep( Params params, Input I )
{
	// Determine total number of tracks
	long ntracks_2D = I.n_azimuthal * (I.assembly_width * sqrt(2) / I.radial_ray_sep);
	long ntracks = ntracks_2D * (I.n_polar_angles * (int) ( I.height / I.axial_z_sep));  

	// calculate the height of a node's domain and of each FSR
	double node_delta_z = I.height / I.decomp_assemblies_ax;
	double source_delta_z = I.cai / I.fai;

	// loop over tracks (implicitly azimuthal angles, tracks in azimuthal angles,
	// polar angles, and z stacked rays)
	for (long i = 0; i < ntracks; i++)
	{
		// get 2D track ID
		long id = params.tracks[i].track2D_id;

		// initialize z and s 
		// (s = current track length progressed along a 2D segment)
		double z = params.tracks[i].z_height;
		double s;

		// allocate an array to store the temporary flux (psi)
		double * psi = (double *) malloc( I.n_egroups * sizeof(double) );
		for( int k = 0; k < I.n_egroups; k++)
			psi[k] = params.tracks[i].start_flux[k];

		// get 2D track segments
		long num_2D_segs = params.tracks_2D[id].n_segments;

		// booleans to determine whether in bounds
		bool in_bounds = true;

		// cycle through all segments in the 2D track laydown
		for (long j =0; j < num_2D_segs; j++)
		{
			bool seg_complete = false;
			s = 0;
			while( !seg_complete )
			{
				// calculate new z coordinate if segment is completed
				double new_z = z + ( params.tracks_2D[id].segments[j].length - s )
				   	/ tan( params.tracks[i].p_angle );

				// check if segment is completed
				if( (int) ( new_z / source_delta_z ) == 
						(int) ( z / source_delta_z) )
					seg_complete = true;

				// otherwise calculate new z coordinate 
				// (hitting the edge of an FSR)
				else
				{
					new_z = (double) ( (int) (z / source_delta_z) );
					if( params.tracks[i].p_angle < M_PI / 2.0)
						new_z += source_delta_z;
					s += ( new_z - z) * tan(params.tracks[i].p_angle);
				}

				// calculate distance traveled in the FSR
				double dist = (new_z - z) / cos(params.tracks[i].p_angle);
					
				// determine if ray is out of bounds
				if( new_z <= 0 || new_z >= node_delta_z)
				{
					in_bounds = false;
					break;
				}

				// pick a random FSR (cache miss expected)
				long source_id = rand() % I.num_source_regions_per_assembly;

				// compute weight (azimuthal * polar)
				//TODO: add track weight (area)
				double weight = params.tracks_2D[id].az_weight * params.tracks[i].p_weight;

				// cycle over energy groups
				for( int k = 0; k < I.n_egroups; k++)
				{
					// load XS data
					double sigT = params.sources[source_id].XS[k][0];
					double nuSigF = params.sources[source_id].XS[k][1];
					double chi = params.sources[source_id].XS[k][2];

					// calculate exponential
					// TODO: Maybe compute (1 - exp) ?? (OpenMOC), also use table lookup
					double exponential = exp( - sigT * dist );

					// calculate change in angular flux
					double delta_psi = (psi[k] - params.sources[source_id].source[k]/sigT) *
						(1.0 - exponential);

					// add contribution to new source flux
					params.sources[source_id].flux[k] += delta_psi * weight;
					
					// update angular flux
					psi[k] -= delta_psi;

				}	

				// TODO: update source scalar flux with source contribution
				// TODO: update source in FSR for next transport sweep

				// set the new z coordinate
				z = new_z;

			}
			if( !in_bounds )
				break;
		}
		free(psi);
	}

	return 0;
}
