// ============================================================================
//  R2-D2 costume helmet — the dome ("the hat")
// ============================================================================
//
//  Rough draft. The top half of R2's head, sized to be worn rather than to be
//  screen accurate, with mounting features for the electronics this repo
//  already drives: the 1.9" TFT logic display and the two PSI pixels.
//
//  THE INSIDE IS SMOOTH. Whatever the outside grows, the interior stays one
//  clean surface — see interior(). It is worn on a head, so it is not a place
//  for the backs of features to end up.
//
//  DESIGN STANCE — the same one the firmware takes. It is a costume, not a
//  replica. Where a screen-accurate detail fights legibility from three metres
//  in a dim room, legibility wins, and the deviation is called out in a comment
//  at the parameter that causes it.
//
//  THE DOME IS MEASURED, NOT GUESSED. Every feature's position and size comes
//  from one of two reference photographs — one from the front, one from behind —
//  converted from pixels to (azimuth, latitude) by the projection in section 2.
//  The pixel numbers are kept in the file so the measurements can be audited and
//  redone against better photos. See REFERENCE FIT.
//
//  Between them the two photos cover azimuth 302 through 50 and 116 through 229.
//  Two wedges on the flanks are in neither, and are filled with plain panels.
//
//  CONVENTIONS
//    · Units are millimetres. Angles are degrees (OpenSCAD's native).
//    · +X is FRONT (the face). +Z is up. Origin sits at the centre of the
//      dome's base plane, so the whole model lives at z >= -ring_h.
//    · Features are placed by (azimuth, latitude), never by hand-typed
//      coordinates. Azimuth 0 is the front centre line; latitude 0 is the base
//      rim and 90 is the apex.
//    · Two ways to put something on the dome, and which one you want depends
//      on whether it has to be FLAT:
//        surface_patch()  follows the curve. Cosmetic panels, seams, the eye
//                         panel. Built in sphere space, squashed with the skin.
//        on_dome()        a local flat frame with +Z along the normal. Bores,
//                         and any pad that has to seat a rigid part.
//
//  RENDERING
//    Preview       F5 in the GUI, or
//                  openscad -o out.png --camera=0,0,45,90,0,90,540 helmet.scad
//    Full render   F6, or add --render=cgal (the `=cgal` is not optional on the
//                  command line — bare --render swallows the filename)
//    Export        openscad -o helmet.stl -D 'part="assembled"' helmet.scad
//
//  Full renders take minutes, almost all of it in the swept panel detail.
//  Preview with show_panels=false while moving things around; it is seconds.
//
//  STATUS / TODO
//    · The two flank wedges are filler, not evidence — a side photo would fix
//      them, and nothing else in the model depends on them.
//    · Whether the back photo is a mirror image is unknowable from the two
//      photos alone; see the note on BACK_PHOTO.
//    · Hardware dimensions marked "MEASURE" come from datasheets and product
//      photos, not calipers. Confirm before printing anything you will keep.
//    · No print-split registration features yet.
//    · No internal mounts for the KB2040, speaker, battery or servo, and
//      nothing to strap it to a head.
//    · The PERISCOPE IS NOT MODELLED. Cut it by hand; the render echoes the
//      azimuth, latitude and flange diameter measured off the photograph.
//    · A smooth interior costs the display pocket its depth — it is 4.3 mm at
//      the middle where it used to be a deep boss, so the module will stand
//      proud of the inner surface and needs something holding it against its
//      lip. Raising fld_proud buys the depth back from the OUTSIDE if that
//      turns out to matter.
//
// ============================================================================


// ============================================================================
//  1. PARAMETERS
// ============================================================================

/* [What to render] */

// assembled = the finished dome; cutaway = quartered to inspect the interior
// and the depth of every pocket; half_front / half_back = split for a printer
// that cannot take the full footprint; quarter_1..4 = split into four, laid on
// a cut face ready to slice; eye_lens = the radar eye lens on its own;
// fit_check = the cutaway with the head keep-out ghosted in, to see clearance.
//
// AT 331 mm THE HALVES NO LONGER FIT a 250 mm bed — a half is D wide. The
// quarters are the printable split; the halves are kept for a larger printer.
part = "assembled";  // ["assembled","cutaway","half_front","half_back","quarter_1","quarter_2","quarter_3","quarter_4","eye_lens","fit_check"]

// Cosmetic detail is most of the render time and none of the fit. Turn it off
// while iterating on the shell.
show_features = true;
show_panels   = true;

$fa = 2;
$fs = 1.2;


/* [Dome shell] */

// Outer diameter at the base rim.
//
// A screen-accurate dome is 18.25" (464 mm) — too big to wear as a hat. A human
// head is roughly 155 mm wide and 195 mm front-to-back, so the interior has to
// clear about 200 mm plus padding. 240 mm outer leaves ~235 mm at the rim.
//
// This is the parameter the whole model hangs off, and it is also the one in
// tension with the hardware: see the FIT WARNING echoed at render time. The
// front features are proportioned from the reference photo, so the TFT, which
// does not scale with them, sets a floor on how small the dome can be.
//
// 331 IS THAT FLOOR, AND IT IS ALSO WHAT THE HEAD NEEDS. The model computed
// 331 as the diameter at which the display housing holds the 5394 at true
// photo proportion, and independently a 125 mm skull needs 129 mm of interior
// height, which 331 provides with 23 mm to spare. Two unrelated constraints
// landing on the same number is the reason to trust it.
//
// It was 240, chosen to print in halves on a 250 mm bed. That is given up: at
// 331 the print is quartered. 71% of screen scale, against 52% before.
dome_dia = 331;

// Vertical squash. Accurate: the real dome is 8.5" tall on 18.25" across.
dome_z_scale = 0.93;

// Shell thickness. 2.4 mm is six perimeters at a 0.4 mm nozzle — enough that
// the dome does not oil-can when it is picked up by the rim.
//
// The Z squash thins the wall at the apex by the same 7% it squashes the dome.
// Immaterial here; worth knowing before thinning this much further.
wall = 2.4;


/* [Base ring] */

ring_h     = 14;   // straight section below the dome
ring_proud = 2.0;  // how far it stands out past the dome diameter
ring_lap   = 2.0;  // how far it laps up over the dome


/* [Head fit] */

// THE HEAD IS MEASURED TOO. Same stance as the dome: these are numbers off a
// tape measure, not anthropometric averages, and the keep-out they describe is
// what every internal part has to clear.
//
// Measured at the top of the ears, which is where a bicycle helmet's band bears
// and where this one is meant to. That level is the BAND LINE, and head_crown_h
// below is measured from it.
head_circ = 580;

// Breadth : length. NOT measured — this is the proportion the dome's own
// comments already assumed (155/195), and it only redistributes a fixed
// circumference between the two axes. Worth measuring if the cradle comes out
// tight on one axis and loose on the other.
head_ratio = 155 / 195;

// Vertical, from the band line straight up to the crown. Measured 120-130; the
// middle of that is taken, since the band bears a few mm above the ear top.
//
// THIS IS THE NUMBER THAT BROKE THE FIT. The dome is 109 mm tall inside and the
// skull above the band is 125, so the head cannot sit with its band at the rim
// — it has to sit lower, which is what head_band_z and a taller ring are for.
head_crown_h = 125;

// SOFT PADDING. A radial allowance all the way round, for the fabric liner and
// a little slip on top of it. Radial, not circumferential: 4 mm here grows the
// circumference by about 25 mm, so the cradle band is cut for ~59.5 cm.
//
// Deliberately separate from head_circ so the measurement stays a measurement.
// Thicker foam, a beanie underneath, or a hot night all live here.
head_pad = 4.0;

// Where the band sits, and how far forward the head is carried. z = 0 is the
// top of the base ring. Dropping the band gains room above the crown and costs
// it at the rim; pushing the head back trades front clearance for rear.
//
// Back at the rim. It was -30 when the dome was 240 and the head did not fit
// under it; at 331 there is room, and a head sitting at the rim is both easier
// to build a cradle for and better balanced than one slung below it.
head_band_z   = 0;
head_offset_x = 0;


/* [Surface relief] */

// Recessed panels and seams both eat into the wall, so both are capped well
// short of it. The assert in section 2 enforces that.
panel_recess = 0.9;
seam_w       = 2.0;
seam_d       = 1.2;

// A dark aperture — the rear display's window, the narrow data slots — reads by
// being deeper than the panel around it. It cannot get that depth by digging:
// there are only 2.4 mm of wall. So the FRAME rises instead, and the apparent
// depth is the frame's height plus the cut, which is also how the reference
// panels are actually built.
panel_aperture    = 1.3;
panel_frame_proud = 1.8;

// How far the raised panels stand out. The reference shows the eye panel as a
// distinctly proud casting and the crown as a shallow step.
//
// The eye panel's height is doing double duty: it is also the material the eye
// socket is sunk into. Panel plus wall has to exceed the socket depth or the
// eye becomes a hole — see the assert in section 2.
eye_panel_proud = 4.5;
crown_proud     = 1.2;

// The radar eye. A shallow dish, not the hemisphere the photo suggests: a
// 34 mm hemisphere is 17 mm deep and there are under 7 mm of material here. The
// depth is faked by the LENS instead, which is a separate part and can bulge as
// far out as it likes.
eye_socket_h  = 4.5;   // recess below the panel face
eye_lens_h    = 8.0;   // how far the lens crowns above the socket floor
eye_lens_seat = 2.0;   // thickness of the disc that locates it
eye_lens_gap  = 0.4;   // slip fit

pie_count = 12;  // radial seams in the crown; one lands on the front centre
                 // line, which is what the reference shows.


// ============================================================================
//  REFERENCE FIT
//
//  A front-elevation photograph of the dome. Fitted by taking the apex and the
//  base ring for the vertical extent and the front centre seam for the centre
//  line, then solving for the horizontal radius from the 0.93 squash. The photo
//  is cropped at both sides, so the horizontal radius is NOT measured directly
//  off a silhouette — it is derived, which is why the fit is worth checking.
//
//  It checks out. Three things the fit predicts, against what was measured
//  independently on the POC before the photo existed:
//
//      crown boundary   fit says lat 54.4      POC had guessed 54
//      PSI lens         fit says 18.8 mm       POC had guessed 20
//      holoprojector    fit says 26.6 mm       POC had guessed 30
//
//  Three independent agreements is enough to trust the rest of the numbers.
// ============================================================================

/* [Reference photo fit — pixels] */

ref_cx = 735;  // front centre line
ref_cy = 800;  // base ring
ref_rx = 793;  // horizontal radius (derived)
ref_ry = 738;  // vertical radius (apex to base)

// Feature boxes, [left, top, right, bottom] in reference pixels. Everything in
// section 5 that is front-facing is driven from one of these.
ref_eye_panel  = [ 510, 232,  962, 600];
ref_greeblie   = [ 903, 255,  958, 512];  // narrow vent at the panel's edge
ref_fld        = [ 265, 515,  500, 792];  // the logic display housing
ref_fld_win    = [ 355, 545,  483, 750];  // both window panes as one block
ref_fld_bar    = [ 278, 528,  345, 780];  // the recessed bar beside them
ref_data_slot  = [ 520, 588,  597, 772];
ref_psi_panel  = [ 608, 590,  902, 776];

// Round features, [centre x, centre y, radius].
ref_eye_lens   = [ 715, 370, 112];  // the black dome itself
ref_eye_socket = [ 715, 370, 137];  // the circular recess it sits in
ref_psi_lens   = [ 790, 658,  62];
ref_holo       = [1015, 655,  88];
ref_holo_lens  = [1015, 655,  30];

ref_crown_y    = 200;   // where the crown meets the body, on the centre line

// The band of plain rectangular panels around the lower dome. Only these four
// are visible in the photo, spanning azimuth -58 to +50; the rest of the ring
// is extrapolated in section 5.
ref_panels = [
    [  75, 545,  245, 790],
    [1128, 520, 1205, 778],
    [1215, 522, 1252, 776],
    [1265, 525, 1332, 774],
];


/* [Front logic display — Adafruit 5394] */

// THE PANEL IS MOUNTED PORTRAIT — long axis up the dome.
//
// The reference housing is taller than it is wide, and the display area inside
// it is split into two stacked panes. A landscape TFT cannot fill that, so the
// panel is turned on its side: 46 mm of glass runs vertically, 25 mm across.
//
// This is a FIRMWARE consequence as much as a mechanical one. The grid in
// display.cpp is drawn 10 cells across by 5 down in the panel's native
// orientation; mounted this way it reads as 5 across by 10 down unless the
// rotation is changed. Nothing in this file can fix that.
fld_portrait = true;

// Visible (active) area, in the panel's own orientation. Long axis first.
fld_panel_long  = 46;
fld_panel_short = 25;

// The WINDOW, deliberately SMALLER than the active area above it.
//
// A window matched to the active area shows the panel's whole perimeter — the
// glass edge, the border around the pixels, the seam where the backlight ends.
// Undersizing the opening laps the lip over all of that instead, and the lip
// is the same piece of bezel that holds the panel down, so it is doing two jobs
// at once.
//
// Long axis is the OUTSIDE of the two panes together: top of the upper to
// bottom of the lower, split bar included.
//
// Costs a margin of pixels at every edge, which is a firmware fact, not a
// mechanical one — see the note at the end of this block.
fld_window_long  = 40;
fld_window_short = 21;

// Module outline — the pocket it drops into from inside. MEASURE.
fld_pcb_long  = 54;
fld_pcb_short = 34;

// Pad material left around the module pocket. This, not the glass, is what
// actually sizes the housing: the 5394's board is 54 x 34 mm and every bit of
// it has to pass through the pad.
fld_pocket_margin = 4;

// The bar dividing the two panes. It is bezel material lying across the middle
// of the glass — which is a bonus, not a cost: Adafruit's warning is that the
// 5394 needs something holding the screen down, and a bar across the centre is
// a better job of that than a rim around the edge.
//
// It lands on the middle cell boundary of the grid, so it clips the edges of
// two cell columns rather than swallowing one whole.
fld_split_bar = 3.0;

// The recessed bar beside the panes. Width is whatever the pad has spare once
// the window block is placed, so it is derived rather than measured.
fld_bar_d = 1.2;

fld_bezel_r = 3;    // corner radius
fld_proud   = 3.5;  // how far the pad stands out from the dome surface

// The lip that overlaps the glass border. Adafruit's warning on the 5394: it
// was designed for a smartwatch where cover glass retains it, and "without
// something gently holding the screen down, the backlight can eventually peel
// away from the TFT." This lip is that something — the panel is pushed against
// it from inside, so retention is structural rather than adhesive.
fld_lip_t = 1.6;


/* [Process State Indicators — the two NeoPixels] */

// The aperture is taken from the reference. The reference PSI sits flush in its
// recessed panel with no bezel of its own, which is what is modelled.
//
// It is a PLAIN BORE, one diameter all the way through. There was a counterbore
// for a diffuser disc to drop into, and the step where it met the through-hole
// read as a lip inside the opening. Nothing to seat a disc against now, so a
// diffuser goes on the back of the hole rather than into it.
// There WAS a shroud tube around each aperture, 14 mm into the dome, to stop
// the front pixel washing the rear lens through a hollow glossy shell — the two
// reading as independent circuits being the entire point of giving them
// separate rhythms in lights.cpp.
//
// It is gone, because the interior is smooth now and the PSIs sit low on the
// dome, right where a head is. That job falls to matte black paint inside, or
// to a tube glued in after the fact.


/* [Reference photo fit — back, pixels] */

// A second photograph, taken from behind. Fitted the same way: apex and base
// ring for the vertical extent, the rear centre seam for the centre line.
//
// HANDEDNESS. Viewed from behind, the horizontal screen axis is reversed
// relative to the front view, and photo_az() already accounts for that through
// the frame's A0 = 180. What it cannot tell you is whether this particular
// photo is a mirror image. Nothing appears in both photographs to check
// against, so if the back turns out reversed on the finished dome, flip the
// sign of the (px - cx) term for this frame — it is a one-line change and
// costs nothing to get wrong on a costume.
ref2_cx = 760;
ref2_cy = 820;
ref2_rx = 808;
ref2_ry = 752;

ref2_crown_y = 215;

// Feature boxes, [left, top, right, bottom] in back-photo pixels. Between them
// the two photographs cover azimuth 302 through 50 (front) and 116 through 229
// (back), leaving two unphotographed wedges on the flanks.
ref2_rld_frame  = [1020, 540, 1360, 805];  // rear logic display, outer frame
ref2_rld_recess = [1040, 555, 1345, 790];
ref2_rld_window = [1105, 590, 1310, 680];  // the dark aperture itself

ref2_psi_frame  = [ 390, 540,  765, 790];  // rear PSI, outer frame
ref2_psi_recess = [ 420, 565,  735, 775];

ref2_plain      = [ 130, 520,  370, 790];  // a bare panel, no fitting in it
ref2_data_slot  = [  50, 555,  115, 790];

// Round features, [centre x, centre y, radius].
ref2_psi_lens   = [523, 662,  86];
ref2_vent_a     = [562, 488,  22];   // the pair of small blind holes above
ref2_vent_b     = [655, 488,  22];   // the PSI panel
ref2_holo       = [895, 650,  95];
ref2_holo_lens  = [895, 650,  30];
// The periscope is NOT modelled — it is left to be cut by hand. The
// measurement is kept anyway: it is evidence off the photograph, and it is
// exactly what tells you where to cut. Echoed at render time for that reason.
ref2_peri       = [558, 155,  80];   // flange where the periscope leaves the dome


// ============================================================================
//  2. DERIVED VALUES
// ============================================================================

// Overlap used wherever two solids or two cuts meet on a shared plane. Small
// enough to be invisible, large enough that no exact-coincidence survives.
face_eps = 0.1;

dome_r  = dome_dia / 2;
dome_rz = dome_r * dome_z_scale;
inner_r = dome_r - wall;

// --- The head ---------------------------------------------------------------
//
// Semi-axes from a circumference, through Ramanujan's ellipse perimeter. At a
// FIXED breadth:length ratio the perimeter is linear in the semi-major axis, so
// this inverts in closed form rather than needing a solve.
//
//     P = PI * a * [3(1+r) - sqrt((3+r)(1+3r))]
function ellipse_a(circ, r) =
    circ / (PI * (3 * (1 + r) - sqrt((3 + r) * (1 + 3 * r))));

head_a = ellipse_a(head_circ, head_ratio);   // fore/aft, bare skin
head_b = head_a * head_ratio;                // lateral,  bare skin

// Padded — what anything inside the dome actually has to clear.
head_ax = head_a + head_pad;
head_by = head_b + head_pad;
head_cz = head_crown_h + head_pad;

head_circ_padded = PI * (3 * (head_ax + head_by)
                   - sqrt((3 * head_ax + head_by) * (head_ax + 3 * head_by)));

// Clearance, as the MINIMUM over the height where head and shell coexist —
// not the value at the band. With the head sitting low the two surfaces are
// different shapes at every height, and the tightest point is not at either
// end. Taking it at the band flattered the fit by several millimetres.
function head_rx(z) =
    z <= head_band_z ? head_ax
  : let (t = (z - head_band_z) / head_cz)
    t >= 1 ? 0 : head_ax * sqrt(1 - t * t);

function head_ry(z) = head_rx(z) * head_by / head_ax;

function shell_r(z) =
    z <= 0 ? inner_r
  : let (t = z / (inner_r * dome_z_scale))
    t >= 1 ? 0 : inner_r * sqrt(1 - t * t);

// Sampled only where there is both shell and skull to measure between.
head_z_lo = -ring_h;
head_z_hi = head_band_z + head_cz;
head_samples = [for (i = [0:60])
                let (z = head_z_lo + (head_z_hi - head_z_lo) * i / 60) z];

head_gap_fore = min([for (z = head_samples)
                     if (head_rx(z) > 0 && shell_r(z) > 0) shell_r(z) - head_rx(z)]);
head_gap_side = min([for (z = head_samples)
                     if (head_ry(z) > 0 && shell_r(z) > 0) shell_r(z) - head_ry(z)]);
head_gap_top  = dome_rz - wall - (head_band_z + head_cz);

// --- Reference projection ---------------------------------------------------
//
// Orthographic view of the squashed dome. For a photo taken from azimuth A0, a
// point at (az, lat) lands at
//     px = cx + rx * cos(lat) * sin(az - A0)
//     py = cy - ry * sin(lat)
// so inverting gives latitude from height alone, and azimuth from horizontal
// offset once latitude is known.
//
// A photo is a frame: [cx, cy, rx, ry, A0]. There are two, taken from opposite
// sides, and everything on the dome is measured in one or the other.
//
// The two are INDEPENDENT fits, which makes them worth cross-checking. Where
// they overlap in what they describe, they agree:
//
//     crown boundary   front lat 54.4   back lat 53.6
//     holoprojector    front 0.111      back 0.117   (of dome diameter)
//
// One thing they do NOT agree on is the PSI lens: 0.078 of the dome at the
// front, 0.106 at the back. That is far outside the error the other two show,
// so it is taken as real — this dome's rear indicator really is the larger of
// the pair — and modelled that way rather than averaged into a compromise.

function photo_lat(f, py) = asin(max(-1, min(1, (f[1] - py) / f[3])));

function photo_az(f, px, py) =
    let (l = photo_lat(f, py))
    f[4] + asin(max(-1, min(1, (px - f[0]) / (f[2] * cos(l)))));

// Reference pixels to millimetres at the CURRENT dome size.
function photo_mm(f, px) = px * dome_dia / (2 * f[2]);

// A pixel box to an ARC: [az_from, az_to, lat_from, lat_to]. Azimuth is taken
// at the box's mid-height, since the same pixel offset spans a wider azimuth
// the higher up the dome you are.
function photo_box(f, b) =
    let (ym = (b[1] + b[3]) / 2)
    [photo_az(f, b[0], ym), photo_az(f, b[2], ym),
     photo_lat(f, b[3]),    photo_lat(f, b[1])];

// Arc dimensions, in millimetres on the current dome.
function arc_w_mm(a) =
    (a[1] - a[0]) * PI / 180 * dome_r * cos((a[2] + a[3]) / 2);
function arc_h_mm(a) = (a[3] - a[2]) * PI / 180 * dome_r;

function arc_mid_az(a)  = (a[0] + a[1]) / 2;
function arc_mid_lat(a) = (a[2] + a[3]) / 2;

// Shrink or grow an arc by a uniform border, in millimetres. Used where the
// photo shows a frame of even width but only one of its two edges was worth
// measuring — the border is then derived rather than invented twice.
function inset_arc(a, m) =
    let (da = arc_deg_at(m, (a[2] + a[3]) / 2), dl = arc_deg(m, dome_r))
    [a[0] + da, a[1] - da, a[2] + dl, a[3] - dl];

// Angular width of `mm` of arc, at a radius, and at a latitude.
function arc_deg(mm, r)      = mm / (r * PI / 180);
function arc_deg_at(mm, lat) = arc_deg(mm, dome_r * cos(lat));

// The two frames, and per-photo shorthands so call sites read cleanly.
FRONT_PHOTO = [ref_cx,  ref_cy,  ref_rx,  ref_ry,    0];
BACK_PHOTO  = [ref2_cx, ref2_cy, ref2_rx, ref2_ry, 180];

function front_box(b) = photo_box(FRONT_PHOTO, b);
function back_box(b)  = photo_box(BACK_PHOTO,  b);
function front_mm(px) = photo_mm(FRONT_PHOTO, px);
function back_mm(px)  = photo_mm(BACK_PHOTO,  px);

crown_lat      = photo_lat(FRONT_PHOTO, ref_crown_y);
crown_lat_back = photo_lat(BACK_PHOTO,  ref2_crown_y);

// --- Flat pads --------------------------------------------------------------
//
// How deep a FLAT pad has to be buried before its corners reach a curved
// surface: the sagitta at its half-diagonal, plus a margin.
//
// Derived rather than typed, because it is the parameter most likely to be
// silently wrong — a pad whose corners float leaves four gaps you do not see in
// a shaded preview and do not find until the print is off the bed.
//
// dome_rz is the SMALLER of the two radii, so this over-buries slightly on the
// flatter parts of the dome. That errs the safe way: too deep costs nothing,
// too shallow is a hole.
function pad_sink(half_diagonal, margin = 2) =
    dome_rz - sqrt(max(1, dome_rz * dome_rz - half_diagonal * half_diagonal))
    + margin;

function half_diag(w, l) = sqrt(w * w + l * l) / 2;

// --- Where the eye panel meets the PSI panel ---------------------------------
//
// These two share a border in the photograph, and a rectangle in (az, lat)
// cannot express the eye panel's real shape — it is a shield that steps inward
// at the bottom, so directly above the PSI its edge sits higher than its
// bounding box says. Read naively the two boxes OVERLAP by 0.8 degrees, because
// the border between them got counted into both.
//
// The 2.5 mm outset that makes the PSI's frame then pushed the overlap to 2.0
// degrees — 4.2 mm — and everything in that strip fought:
//
//   · the raised frame sat on top of the raised eye panel,
//   · each one's outline groove scored a line across the other,
//   · and the PSI's recess bit a notch out of the panel's bottom edge.
//
// Resolved by pushing the eye panel's lower edge clear of the FRAME, not of the
// recess, with room for both outline grooves and material between them. Derived
// rather than nudged in pixels until it looked right, so re-measuring either box
// keeps the relationship.
psi_frame_border = 2.5;
psi_frame_arc    = inset_arc(front_box(ref_psi_panel), -psi_frame_border);

eye_arc = let (m = front_box(ref_eye_panel))
    [m[0], m[1],
     max(m[2], psi_frame_arc[3] + 2 * arc_deg(seam_w, dome_r)),
     m[3]];


// --- The logic display does not scale with the dome -------------------------
//
// Every other front feature is proportioned from the photo, so shrinking the
// dome shrinks it. The TFT is 46 mm of glass whatever the dome does. At 240 mm
// the reference housing is too small to hold it, so the housing is grown to fit
// and the deviation is reported rather than hidden.
// Across the dome / up the dome, once the portrait mounting is applied.
fld_visible_w = fld_portrait ? fld_window_short : fld_window_long;
fld_visible_h = fld_portrait ? fld_window_long  : fld_window_short;
fld_pcb_w     = fld_portrait ? fld_pcb_short    : fld_pcb_long;
fld_pcb_h     = fld_portrait ? fld_pcb_long     : fld_pcb_short;

fld_az = arc_mid_az(front_box(ref_fld));

// Where the window block sits inside the housing. The reference puts it off to
// one side, which is what leaves room for the recessed bar — centring it would
// lose that, so the offset is carried through rather than ignored.
fld_win_dx = front_mm(((ref_fld_win[0] + ref_fld_win[2])
                   - (ref_fld[0]     + ref_fld[2]))     / 2);
fld_win_dy = front_mm(((ref_fld[1]     + ref_fld[3])
                   - (ref_fld_win[1] + ref_fld_win[3])) / 2);

fld_ref_w = arc_w_mm(front_box(ref_fld));
fld_ref_h = arc_h_mm(front_box(ref_fld));

// The pad has to reach around the pocket wherever the pocket ends up.
fld_need_w = 2 * (abs(fld_win_dx) + fld_pcb_w / 2 + fld_pocket_margin);
fld_need_h = 2 * (abs(fld_win_dy) + fld_pcb_h / 2 + fld_pocket_margin);

fld_bezel_w = max(fld_ref_w, fld_need_w);
fld_bezel_l = max(fld_ref_h, fld_need_h);

// Latitude, raised off the measured one if the grown housing would otherwise
// hang past the base rim.
//
// The reference housing sits ON the ring, which is fine — ours grew downward
// past it, and there is no shell below latitude 0 to cut a pocket in. The
// pocket would have opened into the ring band instead of the interior.
fld_lat_ref  = arc_mid_lat(front_box(ref_fld));
fld_half_lat = arc_deg(fld_bezel_l / 2, dome_r);
fld_lat = max(fld_lat_ref,
              arc_deg(fld_pcb_h / 2, dome_r) - arc_deg(fld_win_dy, dome_r) + 1.5);

// Dome diameter at which the reference housing would hold the module unaided —
// the worse of the two axes, since both have to fit.
fld_honest_dia = dome_dia * max(fld_need_w / fld_ref_w, fld_need_h / fld_ref_h);

// Growing the housing has to displace something — the lower band is full. Its
// inboard neighbour is the narrow data slot, so that is what gives way. Dropped
// rather than half-buried: a recess with a bezel sitting across it reads as a
// mistake, and an absent one reads as a design.
fld_az_inboard  = fld_az + arc_deg_at(fld_bezel_w / 2, fld_lat);
fld_pane_h      = (fld_visible_h - fld_split_bar) / 2;

// Does the grown housing run into the eye panel above it?
//
// Tolerated up to a few degrees. The photo itself puts these two within about
// two degrees of each other at their respective mid-heights — they are adjacent
// on the real dome, and two degrees is inside the precision of measuring a
// panel edge off a photograph. Only a real encroachment is worth reporting.
fld_clash_tol = 3;
fld_eye_clash = fld_az_inboard > eye_arc[0] + fld_clash_tol
             && fld_lat + fld_half_lat > eye_arc[2];
data_slot_clear = fld_az_inboard < front_box(ref_data_slot)[0];

fld_sink = pad_sink(half_diag(fld_bezel_w, fld_bezel_l));

// --- Rear, measured off the back photo --------------------------------------

psi_r_dia = back_mm(2 * ref2_psi_lens[2]);
psi_r_az  = photo_az(BACK_PHOTO, ref2_psi_lens[0], ref2_psi_lens[1]);
psi_r_lat = photo_lat(BACK_PHOTO, ref2_psi_lens[1]);

vent_dia = back_mm(2 * ref2_vent_a[2]);

holo_r_dia = back_mm(2 * ref2_holo[2]);

// Where the photograph puts the periscope. Nothing is built from these — see
// the note at ref2_peri — but they are what you need to mark it out by hand.
peri_az_ref   = photo_az(BACK_PHOTO, ref2_peri[0], ref2_peri[1]);
peri_lat_ref  = photo_lat(BACK_PHOTO, ref2_peri[1]);
peri_flange_d = back_mm(2 * ref2_peri[2]);

// --- PSI --------------------------------------------------------------------
psi_dia      = front_mm(2 * ref_psi_lens[2]);
psi_f_az     = photo_az(FRONT_PHOTO, ref_psi_lens[0], ref_psi_lens[1]);
psi_f_lat    = photo_lat(FRONT_PHOTO, ref_psi_lens[1]);


// --- Radar eye --------------------------------------------------------------
eye_az        = photo_az(FRONT_PHOTO, ref_eye_lens[0], ref_eye_lens[1]);
eye_lat       = photo_lat(FRONT_PHOTO, ref_eye_lens[1]);
eye_lens_dia  = front_mm(2 * ref_eye_lens[2]);
eye_socket_d  = front_mm(2 * ref_eye_socket[2]);

// --- Holoprojectors ---------------------------------------------------------
holo_dia = front_mm(2 * ref_holo[2]);

// Proportions rather than absolutes, so the three holoprojectors stay the same
// shape at whatever size each one is.
holo_lens_ratio  = ref_holo_lens[2] / ref_holo[2];
holo_proud_ratio = 0.34;   // not measurable in a head-on photo

// [azimuth, latitude, diameter]. Both are measured off their own photographs
// and come out within 6% of each other, which is the best evidence the two fits
// are compatible.
//
// There was a third at the apex. Neither photograph shows one there — it was
// put in because a dome usually carries three — and on this dome it read as a
// bulb sitting on top rather than as a holoprojector. Removed. If a side photo
// turns one up, it goes back as [azimuth, latitude, diameter] like the others.
holo_positions = [
    [photo_az(FRONT_PHOTO, ref_holo[0], ref_holo[1]),
     photo_lat(FRONT_PHOTO, ref_holo[1]), holo_dia],
    [photo_az(BACK_PHOTO, ref2_holo[0], ref2_holo[1]),
     photo_lat(BACK_PHOTO, ref2_holo[1]), holo_r_dia],
];

// --- Checks -----------------------------------------------------------------

assert(wall > 0.8, "wall is thinner than a printable shell");
assert(max(seam_d, panel_recess, panel_aperture) < wall - 1.0,
       "surface relief would leave less than 1 mm of wall behind it");
assert(fld_pcb_w > fld_visible_w && fld_pcb_h > fld_visible_h,
       "the display pocket must be larger than its own visible area");
assert(fld_pane_h > 4, "the split bar has eaten the window panes");
assert(fld_window_long < fld_panel_long && fld_window_short < fld_panel_short,
       "the window must be smaller than the panel's active area, or the panel's perimeter shows through it");
assert(fld_sink > fld_lip_t, "the display bezel would float off the surface");
assert(eye_socket_h < eye_panel_proud + wall - 1.0,
       "the eye socket would leave under 1 mm of wall behind it");

echo(str("dome: ", dome_dia, " mm across, ", dome_rz, " mm tall, wall ", wall));
echo(str("interior at the rim: ", dome_dia - 2 * wall, " mm"));
echo(str("print footprint: ", dome_dia + 2 * ring_proud, " mm square"));
echo(str("display window ", fld_window_short, " x ", fld_window_long,
         " mm covers ", (fld_panel_short - fld_window_short) / 2,
         " mm of active area down each long edge (about ",
         round(170 * (fld_panel_short - fld_window_short) / 2 / fld_panel_short),
         " px of 170) and ", (fld_panel_long - fld_window_long) / 2,
         " mm at each end (about ",
         round(320 * (fld_panel_long - fld_window_long) / 2 / fld_panel_long),
         " px of 320)."));
echo(str("crown boundary: lat ", crown_lat, " (front photo), ",
         crown_lat_back, " (back photo)"));
echo(str("PSI apertures: front ", psi_dia, " mm, rear ", psi_r_dia,
         " mm — the two photos disagree, and this follows them"));
echo(str("periscope is NOT modelled. The photo puts it at azimuth ", peri_az_ref,
         ", latitude ", peri_lat_ref, ", flange ", peri_flange_d,
         " mm across — mark and cut it by hand."));

echo(str("head: ", head_circ, " mm round becomes ", 2 * head_a, " x ", 2 * head_b,
         " mm; plus ", head_pad, " mm of padding, ", 2 * head_ax, " x ", 2 * head_by,
         " mm (cradle band ", head_circ_padded, " mm round)."));
echo(str("clearance to the shell: ", head_gap_fore, " mm fore/aft, ",
         head_gap_side, " mm lateral, ", head_gap_top, " mm above the crown."));

if (head_gap_top < 0)
    echo(str("FIT WARNING: the head is taller than the dome by ", -head_gap_top,
             " mm. Lower head_band_z, or the crown fouls the apex."));
else if (head_gap_top < 15)
    echo(str("FIT WARNING: only ", head_gap_top,
             " mm above the crown — there is no attic. Anything bulky has to go in the lateral gap beside the ears."));

if (head_gap_fore < 10)
    echo(str("FIT WARNING: ", head_gap_fore,
             " mm fore/aft is not enough for the display pocket plus a cradle wall."));

if (fld_need_w > fld_ref_w || fld_need_h > fld_ref_h)
    echo(str("FIT WARNING: the display housing is grown from ",
             fld_ref_w, " x ", fld_ref_h, " mm (photo proportion) to ",
             fld_bezel_w, " x ", fld_bezel_l,
             " mm to clear the module. A ", fld_honest_dia,
             " mm dome would hold it at true proportion."));

if (!data_slot_clear)
    echo("FIT WARNING: the data slot beside the display is dropped — the oversized housing covers it.");

if (fld_lat > fld_lat_ref + 0.01)
    echo(str("FIT WARNING: the display housing is raised from the measured ",
             fld_lat_ref, " to latitude ", fld_lat,
             " to keep its pocket above the base rim."));

if (fld_eye_clash)
    echo("FIT WARNING: the display housing runs into the eye panel above it. The lower band has no room left at this dome size — everything else on the front is at photo proportion and the display module is not.");


// ============================================================================
//  3. PLACEMENT PRIMITIVES
// ============================================================================

// A point on the dome surface. `lat` is PARAMETRIC latitude: it equals the true
// elevation angle only when dome_z_scale is 1. It is monotonic either way, so
// it stays a perfectly good handle for placing things.
function dome_point(az, lat) = [
    dome_r  * cos(lat) * cos(az),
    dome_r  * cos(lat) * sin(az),
    dome_rz * sin(lat)
];

// Elevation of the surface NORMAL. On a squashed dome this is steeper than the
// latitude itself — the surface is flatter than a sphere's, so its normal
// stands more upright. Getting this right is what makes a flat pad seat without
// a wedge-shaped gap under one edge.
function dome_normal_lat(lat) = atan2(dome_r * sin(lat), dome_rz * cos(lat));

function dome_normal(az, lat) =
    let (n = dome_normal_lat(lat))
    [cos(n) * cos(az), cos(n) * sin(az), sin(n)];

// Place children on the surface in a local frame where +Z is the outward
// normal, +X runs horizontally around the dome and +Y runs up the meridian — so
// a child is drawn the way you would draw it looking straight at that spot. The
// trailing rotate is what buys that; without it +X falls along the meridian and
// every pad lands on its side.
//
// `lift` offsets along the normal. Negative sinks a pad into the shell.
module on_dome(az, lat, lift = 0) {
    translate(dome_point(az, lat) + lift * dome_normal(az, lat))
        rotate([0, 0, az])
            rotate([0, 90 - dome_normal_lat(lat), 0])
                rotate([0, 0, 90])
                    children();
}

// A patch of spherical shell bounded in azimuth and latitude — the primitive
// behind every curved panel and seam on this dome.
//
// SPHERE SPACE. Radii here are pre-squash; the caller is responsible for being
// inside the scale() in section 4. That indirection is what lets a panel be
// swept with rotate_extrude and still come out squashed correctly, which is
// what a panel on a real flattened dome does.
module surface_patch(az1, az2, lat1, lat2, r_in, r_out) {
    n = 14;
    rotate([0, 0, az1])
        rotate_extrude(angle = az2 - az1)
            polygon(concat(
                [for (i = [0:n])
                    let (a = lat1 + (lat2 - lat1) * i / n)
                    [r_out * cos(a), r_out * sin(a)]],
                [for (i = [0:n])
                    let (a = lat2 + (lat1 - lat2) * i / n)
                    [r_in * cos(a), r_in * sin(a)]]
            ));
}

// Raised relief: stands `h` proud, rooted just under the surface.
module raised_patch(az1, az2, lat1, lat2, h) {
    surface_patch(az1, az2, lat1, lat2, dome_r - 0.5, dome_r + h);
}

// Cut relief: a recess or a seam. Overshoots outward so it certainly breaks
// the surface, including through anything raised on top of it.
module cut_patch(az1, az2, lat1, lat2, depth) {
    surface_patch(az1, az2, lat1, lat2, dome_r - depth, dome_r + 30);
}

// The same, driven straight off a reference pixel box.
// The same, driven off a resolved arc — so either photo can supply it.
module raised_arc(a, h)     { raised_patch(a[0], a[1], a[2], a[3], h); }
module cut_arc(a, depth)    { cut_patch(a[0], a[1], a[2], a[3], depth); }

// A groove around an arc's perimeter, straddling its edge.
//
// A raised panel's own step does not read: 4.5 mm of relief on a 240 mm dome
// disappears in flat shading, and on the finished costume it would be carrying
// a paint boundary with no line under it. The reference has a scribed outline
// around every panel, and this is it — it also gives the brush an edge to
// follow.
module arc_outline(a, w = seam_w, d = seam_d) {
    r    = a;
    latm = (r[2] + r[3]) / 2;
    aw   = arc_deg_at(w, latm);   // groove width as an azimuth span
    lw   = arc_deg(w, dome_r);    // and as a latitude span

    cut_patch(r[0] - aw/2, r[0] + aw/2, r[2] - lw, r[3] + lw, d);  // left
    cut_patch(r[1] - aw/2, r[1] + aw/2, r[2] - lw, r[3] + lw, d);  // right
    cut_patch(r[0] - aw/2, r[1] + aw/2, r[2] - lw/2, r[2] + lw/2, d);  // bottom
    cut_patch(r[0] - aw/2, r[1] + aw/2, r[3] - lw/2, r[3] + lw/2, d);  // top
}

// Rounded rectangular prism, base on the XY plane. Used for every flat pad and
// pocket; the radius keeps printed corners from being stress risers.
module rrect_prism(w, l, h, r) {
    hull()
        for (sx = [-1, 1], sy = [-1, 1])
            translate([sx * (w/2 - r), sy * (l/2 - r), 0])
                cylinder(h = h, r = r);
}


// ============================================================================
//  4. SHELL
// ============================================================================

// SOLID, not a shell. The cavity is not cut here — it is cut once, at the very
// end, by interior(). See the note there.
module sphere_shell() {
    difference() {
        sphere(r = dome_r);
        // Keep the top half. Cutting before the Z scale is safe: the scale
        // fixes z = 0, so the cut plane is unmoved by it.
        translate([0, 0, -dome_r]) cube(2 * dome_r + 2, center = true);
    }
}

// The crown steps proud of the body, with the pie seams cut into it. The
// reference shows exactly this: a raised cap, its lower edge a visible step,
// and one radial seam landing on the front centre line.
module crown() {
    raised_patch(0, 360, crown_lat, 90, crown_proud);
}

module crown_seams() {
    // Radial seams stop short of the pole. Run them all the way in and they
    // meet in a degenerate point that renders as a mess and prints as a hole.
    w = arc_deg_at(seam_w, crown_lat);
    for (i = [0 : pie_count - 1])
        cut_patch(i * 360 / pie_count - w / 2,
                  i * 360 / pie_count + w / 2,
                  crown_lat, 84, seam_d);
}

// The band of plain rectangular panels around the lower dome.
//
// The four the photo shows are used as measured. The rest of the ring is
// extrapolated by repeating the widest of them at an even spacing across the
// azimuths the photo does not cover — deliberately plain, so it is obvious
// which panels are evidence and which are filler.
module dome_panels() {
    for (b = ref_panels) cut_arc(front_box(b), panel_recess);

    // The two photographs between them cover azimuth 302 through 50 and 116
    // through 229, leaving two unphotographed wedges on the flanks. These fill
    // them: deliberately plain, evenly spaced, so it stays obvious which panels
    // are evidence and which are filler.
    lo = photo_lat(FRONT_PHOTO, 790);
    hi = photo_lat(FRONT_PHOTO, 530);
    for (c = [64, 83, 102, 244, 263, 282])
        cut_patch(c - 7, c + 7, lo, hi, panel_recess);
}

module base_ring() {
    // Laps `ring_lap` above z = 0 rather than butting against the shell's
    // bottom face. Butting leaves a coincident-face pair, and lapping is what
    // the real dome ring does anyway — a band around the joint, not a spacer
    // under it.
    //
    // Solid, like the shell. interior() hollows it.
    translate([0, 0, -ring_h])
        cylinder(h = ring_h + ring_lap, r = dome_r + ring_proud);
}

// THE INTERIOR, DEFINED ONCE.
//
// Everything the dome carries is differenced against this at the end of
// helmet(), which is what makes the inside smooth whatever the outside grows.
// Without it every sunk pad, bezel and boss pushes its own lump through into
// the cavity — the display housing alone stood 7 mm proud on the inside — and a
// helmet is worn on a head, so the inside is a surface that has to be liveable
// rather than a place for offcuts to end up.
//
// Two pieces. A squashed sphere above the rim, matching the outer surface
// inset by the wall thickness. And a STRAIGHT BORE below it, because the
// sphere is already curving inward there and a rim that narrows as it goes down
// is a rim you cannot get your head through.
module interior() {
    scale([1, 1, dome_z_scale]) sphere(r = inner_r);
    translate([0, 0, -(ring_h + 1)])
        cylinder(h = ring_h + 1 + face_eps, r = inner_r);
}


// THE HEAD KEEP-OUT. Not part of the dome — nothing is cut with it and nothing
// is built from it. It is the volume that has to stay empty, drawn so the
// clearance can be looked at instead of trusted.
//
// Two pieces, for the same reason interior() is two pieces. An ellipsoid for
// the skull above the band, and a straight column below it, because the head
// carries on down to the neck and the dome has to clear all of it — a keep-out
// that stops at the band would quietly permit a cradle that cannot be put on.
//
// $fn is forced: these are unit primitives scaled up, and $fa/$fs would
// tessellate them as though they really were 1 mm across.
module head_keepout() {
    translate([head_offset_x, 0, head_band_z]) {
        scale([head_ax, head_by, head_cz]) sphere(r = 1, $fn = 72);
        translate([0, 0, -(ring_h + 20)])
            scale([head_ax, head_by, 1])
                cylinder(h = ring_h + 20, r = 1, $fn = 72);
    }
}


// ============================================================================
//  5. FEATURES
//
//  Curved relief goes in the *_patch modules and is consumed in sphere space.
//  Anything with a flat seat or a bore goes through on_dome and is consumed
//  afterwards, on the squashed dome. Section 6 keeps the two straight.
// ============================================================================

// --- The eye panel ----------------------------------------------------------
//
// The dominant front feature: a big raised casting spanning roughly 40 degrees
// of azimuth and 35 of latitude, carrying the radar eye near its top and a
// narrow vent at its outboard edge. It follows the dome's curve — at 69 x 73 mm
// a flat pad here would have to bury 12 mm to reach the surface at its corners,
// which is more than the wall is thick.

module eye_panel()   { raised_arc(eye_arc, eye_panel_proud); }
module eye_outline() { arc_outline(eye_arc); }
module eye_vent()    { cut_arc(front_box(ref_greeblie), panel_recess + 1.2); }

// The eye socket: a flat-floored circular recess in the panel face.
module eye_cut() {
    on_dome(eye_az, eye_lat, eye_panel_proud - eye_socket_h)
        cylinder(h = eye_socket_h + 1, d = eye_socket_d);
}

// The lens, as its own part — `part = "eye_lens"`.
//
// A locating disc that fills the socket, with a spherical cap on top. Printed
// dome-up it needs no supports, and the curved face is what gives the
// reference photo's deep highlight; a flat disc reads as a sticker.
//
// Print it black, or paint the cap black and leave the shoulder blue — on the
// reference the black dome sits inside a blue annulus, and the shoulder is
// where that annulus lands.
module eye_lens() {
    a = eye_lens_dia / 2;
    // Sphere radius that gives a cap of half-width `a` and height `eye_lens_h`.
    cap_r = (a * a + eye_lens_h * eye_lens_h) / (2 * eye_lens_h);

    union() {
        cylinder(h = eye_lens_seat, d = eye_socket_d - eye_lens_gap);
        translate([0, 0, eye_lens_seat])
            intersection() {
                translate([0, 0, eye_lens_h - cap_r]) sphere(r = cap_r);
                cylinder(h = eye_lens_h, d = eye_lens_dia + 1);
            }
    }
}

// --- Front logic display ----------------------------------------------------
//
// Stack, from outside in:
//    bezel face  ──┐
//                  │  fld_lip_t   the lip the glass is pressed against
//    seat plane  ──┤
//                  │  pocket      module outline, straight through the shell
//    interior    ──┘
//
// The panel loads from INSIDE and is trapped by the lip. Nothing adhesive is
// holding the display in.
//
// This one is a flat pad, not a curved patch, because the TFT is a rigid sheet
// of glass — the seat has to be a plane whatever the dome is doing underneath.

module fld_add() {
    on_dome(fld_az, fld_lat, -fld_sink)
        rrect_prism(fld_bezel_w, fld_bezel_l,
                    fld_sink + fld_proud, fld_bezel_r);
}

// The recessed bar beside the panes. Sized from whatever the pad has spare on
// the outboard side of the window block, so it tracks the housing rather than
// having to be re-measured every time the pad grows.
module fld_bar() {
    pad_edge = -fld_bezel_w / 2;
    win_edge = fld_win_dx - fld_visible_w / 2;
    avail    = win_edge - pad_edge;
    bar_w    = avail - 2 * 3;   // 3 mm of frame either side of it

    if (bar_w >= 4)
        on_dome(fld_az, fld_lat, fld_proud - fld_bar_d)
            translate([(pad_edge + win_edge) / 2, fld_win_dy, 0])
                rrect_prism(bar_w, fld_visible_h, fld_bar_d + face_eps, 1.2);
}

module fld_cut() {
    seat = fld_proud - fld_lip_t;

    // TWO stacked panes, with the split bar left standing between them. One
    // panel of glass behind both — the division is in the bezel, not the
    // hardware, which is how the reference reads it too.
    //
    // The overshoot past the seat matters. Stopping exactly on it leaves each
    // window's floor coincident with the pocket's ceiling, and coincident faces
    // are ambiguous — the preview z-fights and a CGAL difference can leave a
    // zero-thickness membrane across the opening.
    for (s = [-1, 1])
        on_dome(fld_az, fld_lat, seat - face_eps)
            translate([fld_win_dx,
                       fld_win_dy + s * (fld_pane_h + fld_split_bar) / 2, 0])
                rrect_prism(fld_visible_w, fld_pane_h,
                            fld_lip_t + face_eps + 2, 1.2);

    fld_bar();

    // ONE pocket behind both panes: the module outline, from the seat all the
    // way through, so the header and its wiring have somewhere to go.
    on_dome(fld_az, fld_lat, seat - (fld_sink + wall + 12))
        translate([fld_win_dx, fld_win_dy, 0])
            rrect_prism(fld_pcb_w, fld_pcb_h, fld_sink + wall + 12, 2);
}

// --- Rear panels ------------------------------------------------------------
//
// The back photo shows the same construction as the front, twice over: a
// framed panel with a deeper recess inside it, carrying one fitting. The rear
// logic display's aperture is BLIND — there is no second screen in this build,
// and a hole would leak light out of the dome and rain into it.

// Raised halves, unioned into the shell.
module rear_frames() {
    raised_arc(back_box(ref2_rld_frame), panel_frame_proud);
    raised_arc(back_box(ref2_psi_frame), panel_frame_proud);
    raised_arc(back_box(ref2_plain),     panel_frame_proud);
}

// Cut halves. Apparent depth is panel_frame_proud plus the cut, so the rear
// display's window reads about 3 mm deep while never taking more than 1.3 mm
// out of the wall.
module rear_recesses() {
    arc_outline(back_box(ref2_rld_frame));
    cut_arc(back_box(ref2_rld_recess), panel_recess);
    cut_arc(back_box(ref2_rld_window), panel_aperture);

    arc_outline(back_box(ref2_psi_frame));
    cut_arc(back_box(ref2_psi_recess), panel_recess);

    // The plain panel is a frame around a field that stays near flush — there
    // is no fitting in it. Its border width is derived; the photo shows an even
    // frame and only the outside of it was worth measuring.
    arc_outline(back_box(ref2_plain));
    cut_arc(inset_arc(back_box(ref2_plain), 3), 0.3);

    cut_arc(back_box(ref2_data_slot), panel_aperture);
}

// --- Process State Indicators -----------------------------------------------
//
// Both photos have the PSI sitting flush inside its own recessed rectangular
// panel, with no bezel of its own. The recesses are cut in sphere space; the
// apertures and shrouds are bores on the squashed dome.
//
// The two are NOT the same size — see the note on the reference projection.
// Every module below takes its diameter rather than reading a global, so the
// front and rear can differ without either being a special case.

// The front photo shows a frame here too, but only the recess inside it was
// worth measuring — so the frame is grown from the recess by the same border
// the rear panels use, and the two ends of the dome match.
module psi_frame()  { raised_arc(psi_frame_arc, panel_frame_proud); }
module psi_panel()  { arc_outline(psi_frame_arc);
                      cut_arc(front_box(ref_psi_panel), panel_recess); }
module data_slot()  { cut_arc(front_box(ref_data_slot), panel_aperture); }

module psi_cut(az, lat, dia) {
    // One straight bore, through the panel and the wall. Overshoots well past
    // the inner surface at both ends; interior() trims the inside flush and the
    // panel recess is already open on the outside, so no step is left anywhere.
    on_dome(az, lat, -(wall + 10))
        cylinder(h = wall + 14, d = dia);

}

// The pair of small blind holes above the rear PSI panel. Blind on purpose:
// they are 6.5 mm and there is nothing behind them, so opening them would only
// let light out of the shroud-lit interior.
module rear_vents() {
    for (v = [ref2_vent_a, ref2_vent_b])
        on_dome(photo_az(BACK_PHOTO, v[0], v[1]),
                photo_lat(BACK_PHOTO, v[1]),
                -1.6)
            cylinder(h = 1.6 + face_eps, d = back_mm(2 * v[2]));
}

// --- Holoprojector ----------------------------------------------------------
//
// A stepped cylinder with a small lens, as the reference shows. Modelled normal
// to the surface; the real ones are on ball joints and point wherever they were
// last knocked.

module holo_add(az, lat, dia) {
    proud = dia * holo_proud_ratio;
    step  = dia * 0.78;
    lens  = dia * holo_lens_ratio;
    on_dome(az, lat, -6)
        union() {
            cylinder(h = 6 + proud * 0.45, d = dia);
            translate([0, 0, 6 + proud * 0.45])
                cylinder(h = proud * 0.35, d = step);
            translate([0, 0, 6 + proud * 0.8])
                cylinder(h = proud * 0.2, d1 = step, d2 = lens + 4);
        }
}

module holo_cut(az, lat, dia) {
    proud = dia * holo_proud_ratio;
    on_dome(az, lat, proud - 2.5)
        cylinder(h = 3.5, d = dia * holo_lens_ratio);
}

// ============================================================================
//  6. ASSEMBLY
// ============================================================================

module helmet() {
    difference() {
        union() {
            // Sphere space: the skin and everything that follows its curve.
            scale([1, 1, dome_z_scale])
                difference() {
                    union() {
                        sphere_shell();
                        if (show_panels) {
                            crown();
                            if (show_features) {
                                eye_panel();
                                psi_frame();
                                rear_frames();
                            }
                        }
                    }
                    if (show_panels) {
                        crown_seams();
                        dome_panels();
                        if (show_features) {
                            eye_outline();
                            eye_vent();
                            psi_panel();
                            if (data_slot_clear) data_slot();
                            rear_recesses();
                        }
                    }
                }

            base_ring();

            // Squashed-dome space: flat pads and protrusions.
            if (show_features) {
                fld_add();
                // on_dome, so it belongs HERE and not up with the raised
                // patches — inside the scale() it would be squashed twice.
                for (p = holo_positions) holo_add(p[0], p[1], p[2]);
            }
        }

        // Last, and applied to everything: the smooth inside.
        interior();

        if (show_features) {
            fld_cut();
            psi_cut(psi_f_az, psi_f_lat, psi_dia);
            psi_cut(psi_r_az, psi_r_lat, psi_r_dia);
            rear_vents();
            eye_cut();
            for (p = holo_positions) holo_cut(p[0], p[1], p[2]);
        }
    }
}


// ============================================================================
//  7. OUTPUT
// ============================================================================

// Half-space for the cutaway and the print splits. `keep` is the sign of X to
// retain.
// "quarter_3" -> 2, anything else -> -1.
function quarter_index(p) =
    p == "quarter_1" ? 0 : p == "quarter_2" ? 1 :
    p == "quarter_3" ? 2 : p == "quarter_4" ? 3 : -1;

module keep_half(keep) {
    big = dome_dia * 2;
    intersection() {
        children();
        translate([keep * big / 2, 0, 0]) cube(big, center = true);
    }
}

if (part == "assembled") {
    helmet();

} else if (part == "cutaway") {
    // A quarter removed rather than a half, so the interior is visible while
    // the silhouette is still recognisable.
    difference() {
        helmet();
        translate([0, -dome_dia, -ring_h - 1])
            cube([dome_dia, dome_dia, dome_dia]);
    }

} else if (part == "half_front") {
    keep_half(1) helmet();

} else if (part == "half_back") {
    // Rotated flat-face-down so it is ready to slice without reorienting.
    rotate([0, 0, 180]) keep_half(-1) helmet();

} else if (quarter_index(part) >= 0) {
    // One quarter, laid on a cut face.
    //
    // Rim-down is the wrong orientation for a dome: the overhang worsens all
    // the way up and goes horizontal at the apex. On a cut face the shell
    // stands as a near-vertical wall instead, and the flat face is a perfect
    // first layer. Footprint is about D/2 square by the dome's height.
    rotate([90, 0, 0])
        rotate([0, 0, -90 * quarter_index(part)])
            intersection() {
                helmet();
                translate([0, 0, -ring_h - 1])
                    cube([dome_dia, dome_dia, dome_dia]);
            }

} else if (part == "fit_check") {
    // The cutaway with the head ghosted in. PREVIEW THIS (F5) — the keep-out is
    // drawn with a transparent colour, and a full render discards alpha.
    difference() {
        helmet();
        translate([0, -dome_dia, -ring_h - 1])
            cube([dome_dia, dome_dia, dome_dia]);
    }
    color("salmon", 0.35) head_keepout();

} else if (part == "eye_lens") {
    eye_lens();

} else {
    assert(false, str("unknown part: ", part));
}
