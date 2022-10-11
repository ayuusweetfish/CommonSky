/*
  Naval Observatory Vector Astrometry Software (NOVAS)
  C Edition, Version 3.1

  novascon.c: Constants for use with NOVAS

  U. S. Naval Observatory
  Astronomical Applications Dept.
  Washington, DC
  http://www.usno.navy.mil/USNO/astronomical-applications
*/

/*
   TDB Julian date of epoch J2000.0.
*/

   const double T0 = 2451545.00000000;

/*
   Speed of light in meters/second is a defining physical constant.
*/

   const double C = 299792458.0;

/*
   Astronomical Unit in kilometers.
*/

   const double AU_KM = 1.4959787069098932e+8;

/*
   Angle conversion constants.
*/

   const double ASEC2RAD = 4.848136811095359935899141e-6;
   const double DEG2RAD = 0.017453292519943296;
   const double RAD2DEG = 57.295779513082321;

/*
  Naval Observatory Vector Astrometry Software (NOVAS)
  C Edition, Version 3.1

  novas.h: Header file for novas.c

  U. S. Naval Observatory
  Astronomical Applications Dept.
  Washington, DC
  http://www.usno.navy.mil/USNO/astronomical-applications
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/*
   Structures
*/

/*
   struct cat_entry:  basic astrometric data for any celestial object
                      located outside the solar system; the catalog
                      data for a star
   starname[SIZE_OF_OBJ_NAME] = name of celestial object
   catalog[SIZE_OF_CAT_NAME]  = catalog designator (e.g., HIP)
   starnumber                 = integer identifier assigned to object
   ra                         = ICRS right ascension (hours)
   dec                        = ICRS declination (degrees)
   promora                    = ICRS proper motion in right ascension
                                (milliarcseconds/year)
   promodec                   = ICRS proper motion in declination
                                (milliarcseconds/year)
   parallax                   = parallax (milliarcseconds)
   radialvelocity             = radial velocity (km/s)
   SIZE_OF_OBJ_NAME and SIZE_OF_CAT_NAME are defined below.  Each is the
   number of characters in the string (the string length) plus the null
   terminator.
*/

   #define SIZE_OF_OBJ_NAME 51
   #define SIZE_OF_CAT_NAME 4

   typedef struct
   {
      char starname[SIZE_OF_OBJ_NAME];
      char catalog[SIZE_OF_CAT_NAME];
      long int starnumber;
      double ra;
      double dec;
      double promora;
      double promodec;
      double parallax;
      double radialvelocity;
   } cat_entry;

/*
   Function prototypes
*/

   short int precession (double jd_tdb1, double *pos1, double jd_tdb2,

                         double *pos2);

   void transform_hip (cat_entry *hipparcos,

                       cat_entry *hip_2000);

   short int transform_cat (short int option, double date_incat,
                            cat_entry *incat, double date_newcat,
                            char newcat_id[SIZE_OF_CAT_NAME],

                            cat_entry *newcat);

/*
  Naval Observatory Vector Astrometry Software (NOVAS)
  C Edition, Version 3.1

  novas.c: Main library

  U. S. Naval Observatory
  Astronomical Applications Dept.
  Washington, DC
  http://www.usno.navy.mil/USNO/astronomical-applications
*/

/********precession */

short int precession (double jd_tdb1, double *pos1, double jd_tdb2,

                      double *pos2)
/*
------------------------------------------------------------------------

   PURPOSE:
      Precesses equatorial rectangular coordinates from one epoch to
      another.  One of the two epochs must be J2000.0.  The coordinates
      are referred to the mean dynamical equator and equinox of the two
      respective epochs.

   REFERENCES:
      Explanatory Supplement To The Astronomical Almanac, pp. 103-104.
      Capitaine, N. et al. (2003), Astronomy And Astrophysics 412,
         pp. 567-586.
      Hilton, J. L. et al. (2006), IAU WG report, Celest. Mech., 94,
         pp. 351-367.

   INPUT
   ARGUMENTS:
      jd_tdb1 (double)
         TDB Julian date of first epoch.  See Note 1 below.
      pos1[3] (double)
         Position vector, geocentric equatorial rectangular coordinates,
         referred to mean dynamical equator and equinox of first epoch.
      jd_tdb2 (double)
         TDB Julian date of second epoch.  See Note 1 below.

   OUTPUT
   ARGUMENTS:
      pos2[3] (double)
         Position vector, geocentric equatorial rectangular coordinates,
         referred to mean dynamical equator and equinox of second epoch.

   RETURNED
   VALUE:
      (short int)
         = 0 ... everything OK.
         = 1 ... Precession not to or from J2000.0; 'jd_tdb1' or 'jd_tdb2'
                 not 2451545.0.

   GLOBALS
   USED:
      T0, ASEC2RAD       novascon.c

   FUNCTIONS
   CALLED:
      fabs               math.h
      sin                math.h
      cos                math.h

   VER./DATE/
   PROGRAMMER:
      V1.0/01-93/TKB (USNO/NRL Optical Interfer.) Translate Fortran.
      V1.1/08-93/WTH (USNO/AA) Update to C Standards.
      V1.2/03-98/JAB (USNO/AA) Change function type from 'short int' to
                               'void'.
      V1.3/12-99/JAB (USNO/AA) Precompute trig terms for greater
                               efficiency.
      V2.0/10-03/JAB (USNO/AA) Update function for consistency with
                               IERS (2000) Conventions.
      V2.1/01-05/JAB (USNO/AA) Update expressions for the precession
                               angles (extra significant digits).
      V2.2/04-06/JAB (USNO/AA) Update model to 2006 IAU convention.
                               This is model "P03" of second reference.
      V2.3/03-10/JAB (USNO/AA) Implement 'first-time' to fix bug when
                                'jd_tdb2' is 'T0' on first call to
                                function.

   NOTES:
      1. Either 'jd_tdb1' or 'jd_tdb2' must be 2451545.0 (J2000.0) TDB.
      2. This function is the C version of NOVAS Fortran routine
      'preces'.

------------------------------------------------------------------------
*/
{
   static short int first_time = 1;
   short int error = 0;

   static double t_last = 0.0;
   static double xx, yx, zx, xy, yy, zy, xz, yz, zz;
   double eps0 = 84381.406;
   double  t, psia, omegaa, chia, sa, ca, sb, cb, sc, cc, sd, cd;

/*
   Check to be sure that either 'jd_tdb1' or 'jd_tdb2' is equal to T0.
*/

   if ((jd_tdb1 != T0) && (jd_tdb2 != T0))
      return (error = 1);

/*
   't' is time in TDB centuries between the two epochs.
*/

   t = (jd_tdb2 - jd_tdb1) / 36525.0;

   if (jd_tdb2 == T0)
      t = -t;

   if ((fabs (t - t_last) >= 1.0e-15) || (first_time == 1))
   {

/*
   Numerical coefficients of psi_a, omega_a, and chi_a, along with
   epsilon_0, the obliquity at J2000.0, are 4-angle formulation from
   Capitaine et al. (2003), eqs. (4), (37), & (39).
*/

      psia   = ((((-    0.0000000951  * t
                   +    0.000132851 ) * t
                   -    0.00114045  ) * t
                   -    1.0790069   ) * t
                   + 5038.481507    ) * t;

      omegaa = ((((+    0.0000003337  * t
                   -    0.000000467 ) * t
                   -    0.00772503  ) * t
                   +    0.0512623   ) * t
                   -    0.025754    ) * t + eps0;

      chia   = ((((-    0.0000000560  * t
                   +    0.000170663 ) * t
                   -    0.00121197  ) * t
                   -    2.3814292   ) * t
                   +   10.556403    ) * t;

      eps0 = eps0 * ASEC2RAD;
      psia = psia * ASEC2RAD;
      omegaa = omegaa * ASEC2RAD;
      chia = chia * ASEC2RAD;

      sa = sin (eps0);
      ca = cos (eps0);
      sb = sin (-psia);
      cb = cos (-psia);
      sc = sin (-omegaa);
      cc = cos (-omegaa);
      sd = sin (chia);
      cd = cos (chia);
/*
   Compute elements of precession rotation matrix equivalent to
   R3(chi_a) R1(-omega_a) R3(-psi_a) R1(epsilon_0).
*/

      xx =  cd * cb - sb * sd * cc;
      yx =  cd * sb * ca + sd * cc * cb * ca - sa * sd * sc;
      zx =  cd * sb * sa + sd * cc * cb * sa + ca * sd * sc;
      xy = -sd * cb - sb * cd * cc;
      yy = -sd * sb * ca + cd * cc * cb * ca - sa * cd * sc;
      zy = -sd * sb * sa + cd * cc * cb * sa + ca * cd * sc;
      xz =  sb * sc;
      yz = -sc * cb * ca - sa * cc;
      zz = -sc * cb * sa + cc * ca;

      t_last = t;
      first_time = 0;
   }

   if (jd_tdb2 == T0)
   {

/*
   Perform rotation from epoch to J2000.0.
*/
      pos2[0] = xx * pos1[0] + xy * pos1[1] + xz * pos1[2];
      pos2[1] = yx * pos1[0] + yy * pos1[1] + yz * pos1[2];
      pos2[2] = zx * pos1[0] + zy * pos1[1] + zz * pos1[2];
   }
    else
   {

/*
   Perform rotation from J2000.0 to epoch.
*/

      pos2[0] = xx * pos1[0] + yx * pos1[1] + zx * pos1[2];
      pos2[1] = xy * pos1[0] + yy * pos1[1] + zy * pos1[2];
      pos2[2] = xz * pos1[0] + yz * pos1[1] + zz * pos1[2];
   }

   return (error = 0);
}

/********transform_hip */

void transform_hip (cat_entry *hipparcos,

                    cat_entry *hip_2000)
/*
------------------------------------------------------------------------

   PURPOSE:
      To convert Hipparcos catalog data at epoch J1991.25 to epoch
      J2000.0, for use within NOVAS.  To be used only for Hipparcos or
      Tycho stars with linear space motion.  Both input and output data
      is in the ICRS.

   REFERENCES:
      None.

   INPUT
   ARGUMENTS:
      *hipparcos (struct cat_entry)
         An entry from the Hipparcos catalog, at epoch J1991.25, with
         all members having Hipparcos catalog units.  See Note 1
         below (struct defined in novas.h).

   OUTPUT
   ARGUMENTS:
      *hip_2000 (struct cat_entry)
         The transformed input entry, at epoch J2000.0.  See Note 2
         below (struct defined in novas.h).


   RETURNED
   VALUE:
      None.

   GLOBALS
   USED:
      T0                 novascon.c

   FUNCTIONS
   CALLED:
      transform_cat      novas.c

   VER./DATE/
   PROGRAMMER:
      V1.0/03-98/JAB (USNO/AA)
      V1.1/11-03/JAB (USNO/AA) Update for ICRS

   NOTES:
      1. Input (Hipparcos catalog) epoch and units:
         Epoch: J1991.25
         Right ascension (RA): degrees
         Declination (Dec): degrees
         Proper motion in RA: milliarcseconds per year
         Proper motion in Dec: milliarcseconds per year
         Parallax: milliarcseconds
         Radial velocity: kilometers per second (not in catalog)
      2. Output (modified Hipparcos) epoch and units:
         Epoch: J2000.0
         Right ascension: hours
         Declination: degrees
         Proper motion in RA: milliarcseconds per year
         Proper motion in Dec: milliarcseconds per year
         Parallax: milliarcseconds
         Radial velocity: kilometers per second
      3. This function is the C version of NOVAS Fortran routine
         'gethip'.

------------------------------------------------------------------------
*/
{
   const double epoch_hip = 2448349.0625;

   cat_entry scratch;

/*
   Set up a "scratch" catalog entry containing Hipparcos data in
   "NOVAS units."
*/

   strcpy (scratch.starname, hipparcos->starname);
   scratch.starnumber = hipparcos->starnumber;
   scratch.dec = hipparcos->dec;
   scratch.promora = hipparcos->promora;
   scratch.promodec = hipparcos->promodec;
   scratch.parallax = hipparcos->parallax;
   scratch.radialvelocity = hipparcos->radialvelocity;

   strcpy (scratch.catalog, "SCR");

/*
   Convert right ascension from degrees to hours.
*/

   scratch.ra = hipparcos->ra / 15.0;

/*
   Change the epoch of the Hipparcos data from J1991.25 to J2000.0.
*/

   transform_cat (1,epoch_hip,&scratch,T0,"HP2", hip_2000);

   return;
}

/********transform_cat */

short int transform_cat (short int option, double date_incat,
                         cat_entry *incat, double date_newcat,
                         char newcat_id[SIZE_OF_CAT_NAME],

                         cat_entry *newcat)
/*
------------------------------------------------------------------------

   PURPOSE:
      To transform a star's catalog quantities for a change of epoch
      and/or equator and equinox.  Also used to rotate catalog
      quantities on the dynamical equator and equinox of J2000.0 to the
      ICRS or vice versa.

   REFERENCES:
      None.

   INPUT
   ARGUMENTS:
      option (short int)
         Transformation option
            = 1 ... change epoch; same equator and equinox
            = 2 ... change equator and equinox; same epoch
            = 3 ... change equator and equinox and epoch
            = 4 ... change equator and equinox J2000.0 to ICRS
            = 5 ... change ICRS to equator and equinox of J2000.0
      date_incat (double)
         TT Julian date, or year, of input catalog data.
      *incat (struct cat_entry)
         An entry from the input catalog, with units as given in
         the struct definition (struct defined in novas.h).
      date_newcat (double)
         TT Julian date, or year, of transformed catalog data.
      newcat_id[SIZE_OF_CAT_NAME] (char)
         Catalog identifier ((SIZE_OF_CAT_NAME - 1) characters maximum)
         e.g. HIP = Hipparcos, TY2 = Tycho-2.

   OUTPUT
   ARGUMENTS:
      *newcat (struct cat_entry)
         The transformed catalog entry, with units as given in
         the struct definition (struct defined in novas.h).

   RETURNED
   VALUE:
      = 0 ... Everything OK.
      = 1 ... Invalid value of an input date for option 2 or 3 (see
              Note 1 below).
      = 2 ... length of 'newcat_id' out of bounds.

   GLOBALS
   USED:
      SIZE_OF_CAT_NAME   novas.h
      T0, ASEC2RAD       novascon.c
      AU_KM, C           novascon.c

   FUNCTIONS
   CALLED:
      precession         novas.c
      frame_tie          novas.c
      sin                math.h
      cos                math.h
      sqrt               math.h
      atan2              math.h
      asin               math.h
      strcpy             string.h

   VER./DATE/
   PROGRAMMER:
      V1.0/03-98/JAB (USNO/AA)
      V1.1/11-03/JAB (USNO/AA) Update for ICRS; add options 4 and 5.
      V1.2/06-08/WKP (USNO/AA) Changed 'a[3]' to 'd', added Doppler factor,
                               and corrected 'frame_tie' direction.
      V1.3/06-08/JAB (USNO/AA) Error check on dates for options 2 and 3.
      V1.4/02-11/WKP (USNO/AA) Implement SIZE_OF_CAT_NAME.

   NOTES:
      1. 'date_incat' and 'date_newcat' may be specified either as a
      Julian date (e.g., 2433282.5) or a Julian year and fraction
      (e.g., 1950.0).  Values less than 10000 are assumed to be years.
      For 'option' = 2 or 'option' = 3, either 'date_incat' or
      'date_newcat' must be 2451545.0 or 2000.0 (J2000.0).  For
      'option' = 4 and 'option' = 5, 'date_incat' and 'date_newcat'
      are ignored.
      2. 'option' = 1 updates the star's data to account for the
      star's space motion between the first and second dates, within a
      fixed reference frame.
         'option' = 2 applies a rotation of the reference frame
      corresponding to precession between the first and second dates,
      but leaves the star fixed in space.
         'option' = 3 provides both transformations.
         'option' = 4 and 'option' = 5 provide a a fixed rotation
      about very small angles (<0.1 arcsecond) to take data from the
      dynamical system of J2000.0 to the ICRS ('option' = 4) or vice
      versa ('option' = 5).
      3. For 'option' = 1, input data can be in any fixed reference
      system. for 'option' = 2 or 'option' = 3, this function assumes
      the input data is in the dynamical system and produces output in
      the dynamical system.  for 'option' = 4, the input data must be
      on the dynamical equator and equinox of J2000.0.  for
     'option' = 5, the input data must be in the ICRS.
      4. This function cannot be properly used to bring data from
      old star catalogs into the modern system, because old catalogs
      were compiled using a set of constants that are
      incompatible with modern values.  In particular, it should not
      be used for catalogs whose positions and proper motions were
      derived by assuming a precession constant significantly different
      from the value implicit in function 'precession'.
      5. This function is the C version of NOVAS Fortran routine
      'catran'.
      6. SIZE_OF_CAT_NAME is defined in novas.h.

------------------------------------------------------------------------
*/
{
   short int error = 0;
   short int j;

   double jd_incat, jd_newcat, paralx, dist, r, d, cra, sra, cdc,
      sdc, k, pos1[3], term1, pmr, pmd, rvl, vel1[3], pos2[3],
      vel2[3], xyproj;

/*
   If necessary, compute Julian dates.

   This function uses TDB Julian dates internally, but no
   distinction between TDB and TT is necessary.
*/

   if (date_incat < 10000.0)
      jd_incat = T0 + (date_incat - 2000.0) * 365.25;
    else
      jd_incat = date_incat;

   if (date_newcat < 10000.0)
      jd_newcat = T0 + (date_newcat - 2000.0) * 365.25;
    else
      jd_newcat = date_newcat;

/*
   Convert input angular components to vectors

   If parallax is unknown, undetermined, or zero, set it to 1.0e-6
   milliarcsecond, corresponding to a distance of 1 gigaparsec.
*/

   paralx = incat->parallax;
   if (paralx <= 0.0)
      paralx = 1.0e-6;

/*
   Convert right ascension, declination, and parallax to position
   vector in equatorial system with units of AU.
*/

   dist = 1.0 / sin (paralx * 1.0e-3 * ASEC2RAD);
   r = incat->ra * 54000.0 * ASEC2RAD;
   d = incat->dec * 3600.0 * ASEC2RAD;
   cra = cos (r);
   sra = sin (r);
   cdc = cos (d);
   sdc = sin (d);
   pos1[0] = dist * cdc * cra;
   pos1[1] = dist * cdc * sra;
   pos1[2] = dist * sdc;

/*
   Compute Doppler factor, which accounts for change in light
   travel time to star.
*/

   k = 1.0 / (1.0 - incat->radialvelocity / C * 1000.0);

/*
   Convert proper motion and radial velocity to orthogonal components
   of motion, in spherical polar system at star's original position,
   with units of AU/day.
*/

   term1 = paralx * 365.25;
   pmr = incat->promora  / term1 * k;
   pmd = incat->promodec / term1 * k;
   rvl = incat->radialvelocity * 86400.0 / AU_KM * k;

/*
   Transform motion vector to equatorial system.
*/

   vel1[0] = - pmr * sra - pmd * sdc * cra + rvl * cdc * cra;
   vel1[1] =   pmr * cra - pmd * sdc * sra + rvl * cdc * sra;
   vel1[2] =               pmd * cdc       + rvl * sdc;

/*
   Update star's position vector for space motion (only if 'option' = 1
   or 'option' = 3).
*/

   if ((option == 1) || (option == 3))
   {
      for (j = 0; j < 3; j++)
      {
         pos2[j] = pos1[j] + vel1[j] * (jd_newcat - jd_incat);
         vel2[j] = vel1[j];
      }
   }
    else
   {
      for (j = 0; j < 3; j++)
      {
           pos2[j] = pos1[j];
           vel2[j] = vel1[j];
      }
   }

/*
   Precess position and velocity vectors (only if 'option' = 2 or
   'option' = 3).
*/

   if ((option == 2) || (option == 3))
   {
      for (j = 0; j < 3; j++)
      {
           pos1[j] = pos2[j];
           vel1[j] = vel2[j];
      }
      if ((error = precession (jd_incat,pos1,jd_newcat, pos2)) != 0)
         return (error);
      precession (jd_incat,vel1,jd_newcat, vel2);
   }

/*
   Rotate dynamical J2000.0 position and velocity vectors to ICRS
   (only if 'option' = 4).
*/

/*
   if (option == 4)
   {
      frame_tie (pos1,-1, pos2);
      frame_tie (vel1,-1, vel2);
   }
*/

/*
   Rotate ICRS position and velocity vectors to dynamical J2000.0
   (only if 'option' = 5).
*/
/*
   if (option == 5)
   {
      frame_tie (pos1,1, pos2);
      frame_tie (vel1,1, vel2);
   }
*/

/*
   Convert vectors back to angular components for output.

   From updated position vector, obtain star's new position expressed
   as angular quantities.
*/

   xyproj = sqrt (pos2[0] * pos2[0] + pos2[1] * pos2[1]);

   if (xyproj > 0.0)
      r = atan2 (pos2[1], pos2[0]);
    else
      r = 0.0;
   newcat->ra = r / ASEC2RAD / 54000.0;
   if (newcat->ra < 0.0)
      newcat->ra += 24.0;
   if (newcat->ra >= 24.0)
      newcat->ra -= 24.0;

   d = atan2 (pos2[2], xyproj);
   newcat->dec = d / ASEC2RAD / 3600.0;

   dist = sqrt (pos2[0] * pos2[0] + pos2[1] * pos2[1] +
      pos2[2] * pos2[2]);

   paralx = asin (1.0 / dist) / ASEC2RAD * 1000.0;
   newcat->parallax = paralx;

/*
   Transform motion vector back to spherical polar system at star's
   new position.
*/

   cra = cos (r);
   sra = sin (r);
   cdc = cos (d);
   sdc = sin (d);
   pmr = - vel2[0] * sra       + vel2[1] * cra;
   pmd = - vel2[0] * cra * sdc - vel2[1] * sra * sdc + vel2[2] * cdc;
   rvl =   vel2[0] * cra * cdc + vel2[1] * sra * cdc + vel2[2] * sdc;

/*
   Convert components of motion to from AU/day to normal catalog units.
*/

   newcat->promora  = pmr * paralx * 365.25 / k;
   newcat->promodec = pmd * paralx * 365.25 / k;
   newcat->radialvelocity = rvl * (AU_KM / 86400.0) / k;

/*
  Take care of zero-parallax case.
*/

   if (newcat->parallax <= 1.01e-6)
   {
      newcat->parallax = 0.0;
      newcat->radialvelocity = incat->radialvelocity;
   }

/*
   Set the catalog identification code for the transformed catalog
   entry.
*/

   if ((short int) strlen (newcat_id) > SIZE_OF_CAT_NAME - 1)
      return (2);
    else
      strcpy (newcat->catalog, newcat_id);

/*
   Copy unchanged quantities from the input catalog entry to the
   transformed catalog entry.
*/

   strcpy (newcat->starname, incat->starname);
   newcat->starnumber = incat->starnumber;

   return (error);
}

// Entry

int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <hip2.dat>\n", argv[0]);
    return 0;
  }

  FILE *fi = fopen(argv[1], "r");
  if (!fi) {
    fprintf(stderr, "Cannot open input <%s>\n", argv[1]);
    return 1;
  }

  // http://simbad.u-strasbg.fr/simbad/sim-id?Ident=HIP+80763
/*
  cat_entry hip = {
    .starnumber = 80763,
    .ra = 4.3171059089 * RAD2DEG,
    .dec = -0.4613244851 * RAD2DEG,
    .promora = -12.11,
    .promodec = -23.30,
    .parallax = 5.89,
    .radialvelocity = 0,
  };
  cat_entry hip_j2000;
  transform_hip(&hip, &hip_j2000);
  hip_j2000.ra *= 15;
  printf("%.12lf %.12lf\n", hip.ra, hip.dec);
  printf("%.12lf %.12lf\n", hip_j2000.ra, hip_j2000.dec);
  printf("%d %d %.6lf\n",
    (int)(hip_j2000.ra / 15),
    (int)fabs(fmod(hip_j2000.ra, 15) * 4),
    fabs(fmod(hip_j2000.ra * 4, 1) * 60));
  printf("%d %d %.6lf\n",
    (int)(hip_j2000.dec),
    (int)fabs(fmod(hip_j2000.dec, 1) * 60),
    fabs(fmod(hip_j2000.dec * 60, 1) * 60));
*/

  // http://simbad.cds.unistra.fr/simbad/sim-ref?bibcode=2007A%26A...474..653V
  // cdsarc.u-strasbg.fr/viz-bin/cat/I/311
  char s[278];
  while (fgets(s, sizeof s, fi) != NULL) {
    int id = (int)strtol(&s[0], NULL, 10);
    double ra = (double)strtod(&s[15], NULL);
    double dec = (double)strtod(&s[29], NULL);
    double plx = (double)strtod(&s[43], NULL);
    double pmra = (double)strtod(&s[51], NULL);
    double pmdec = (double)strtod(&s[60], NULL);

    cat_entry hip = {
      .ra = ra * RAD2DEG,
      .dec = dec * RAD2DEG,
      .promora = pmra,
      .promodec = pmdec,
      .parallax = plx,
      .radialvelocity = 0,
    };
    cat_entry hip_j2000;
    transform_hip(&hip, &hip_j2000);
    hip_j2000.ra *= 15;
    fprintf(stderr, "%6d  %3d %2d %12.9lf  %3d %2d %12.9lf\n",
      id,
      (int)(hip_j2000.ra / 15),
      (int)fabs(fmod(hip_j2000.ra, 15) * 4),
      fabs(fmod(hip_j2000.ra * 4, 1) * 60),
      (int)(hip_j2000.dec),
      (int)fabs(fmod(hip_j2000.dec, 1) * 60),
      fabs(fmod(hip_j2000.dec * 60, 1) * 60));
    fprintf(stdout, "%d %.9lf %.9lf\n", id, hip_j2000.ra, hip_j2000.dec);
  }

  fclose(fi);

  return 0;
}
