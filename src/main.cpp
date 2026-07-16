#include <AMReX.H>
#include <AMReX_ParmParse.H>
#include <AMReX_PlotFileUtil.H>
#include <AMReX_Geometry.H>
#include <AMReX_MultiFab.H>
#include <AMReX_Vector.H>
#include <AMReX_RealBox.H>
#include <AMReX_ParallelDescriptor.H>
#include <AMReX_Print.H>
#include <AMReX_BLProfiler.H>

#ifdef AMREX_USE_HDF5
#include <AMReX_PlotFileUtilHDF5.H>
#endif

#include <string>
#include <vector>

int main(int argc, char* argv[])
{
    amrex::Initialize(argc, argv);
    {
        BL_PROFILE("main");

        // -------------------------------------------------------------------
        // Parse inputs
        // -------------------------------------------------------------------
        amrex::ParmParse pp;

        // List of input native AMReX plotfiles
        amrex::Vector<std::string> plotfiles;
        pp.queryarr("plotfiles", plotfiles);

        // Accept plotfile names directly from the command line as positional
        // arguments (any argument that does not contain '=').
        for (int i = 1; i < argc; ++i) {
            std::string arg(argv[i]);
            // Skip ParmParse-style inputs files and key=value pairs
            if (arg.find('=') == std::string::npos &&
                arg.find("inputs") == std::string::npos)
            {
                // Only add if it looks like a directory/file path (not a flag)
                if (arg[0] != '-') {
                    plotfiles.push_back(arg);
                }
            }
        }

        if (plotfiles.empty()) {
            amrex::Abort(
                "No input plotfiles specified.\n"
                "Usage: amrex-plotfile-compresser inputs/example.inp\n"
                "       or: amrex-plotfile-compresser plotfiles=plt00000 plt01000\n"
            );
        }

        // HDF5 compression descriptor
        // Supported: None@0, ZLIB@<level>, ZFP_RATE@<rate>,
        //            ZFP_PRECISION@<precision>, ZFP_ACCURACY@<accuracy>,
        //            ZFP_REVERSIBLE@reversible
        std::string hdf5_compression = "ZFP_ACCURACY@0.001";
        pp.query("hdf5_compression", hdf5_compression);

        // Optional output prefix; if empty, derived from input name
        std::string output_prefix;
        pp.query("output_prefix", output_prefix);

#ifndef AMREX_USE_HDF5
        amrex::Abort(
            "This application requires AMReX to be built with HDF5 support "
            "(AMReX_HDF5=ON). Please reconfigure and rebuild."
        );
#endif

        const bool is_io_proc = amrex::ParallelDescriptor::IOProcessor();

        if (is_io_proc) {
            amrex::Print() << "\n"
                           << "=================================================\n"
                           << " amrex-plotfile-compresser\n"
                           << "=================================================\n"
                           << " HDF5 compression : " << hdf5_compression << "\n"
                           << " Number of files  : " << plotfiles.size() << "\n"
                           << "=================================================\n\n";
        }

        // -------------------------------------------------------------------
        // Process each plotfile
        // -------------------------------------------------------------------
        for (std::size_t ipf = 0; ipf < plotfiles.size(); ++ipf)
        {
            const std::string& infile = plotfiles[ipf];

            // Determine output filename
            std::string outfile;
            if (!output_prefix.empty()) {
                // Strip any trailing slashes/path separators from infile base
                std::string base = infile;
                while (!base.empty() && base.back() == '/') {
                    base.pop_back();
                }
                // Take the last path component
                auto slash = base.rfind('/');
                std::string leaf = (slash == std::string::npos) ? base : base.substr(slash + 1);
                outfile = output_prefix + "_" + leaf + ".h5";
            } else {
                // Default: input name with .h5 appended
                std::string base = infile;
                while (!base.empty() && base.back() == '/') {
                    base.pop_back();
                }
                outfile = base + ".h5";
            }

            if (is_io_proc) {
                amrex::Print() << "[" << (ipf + 1) << "/" << plotfiles.size() << "] "
                               << infile << " -> " << outfile << "\n";
            }

            amrex::Real t_start = amrex::ParallelDescriptor::second();

            // ---------------------------------------------------------------
            // Read native plotfile
            // ---------------------------------------------------------------
            amrex::PlotFileData pfd(infile);

            const int finest_level = pfd.finestLevel();
            const int nlevels      = finest_level + 1;
            const amrex::Real sim_time = pfd.time();
            const int ncomp = pfd.nComp();
            const amrex::Vector<std::string>& varnames = pfd.varNames();
            const int coord_sys = pfd.coordSys();

            auto probLo_arr = pfd.probLo();
            auto probHi_arr = pfd.probHi();

            amrex::RealBox real_box(probLo_arr.data(), probHi_arr.data());

            // Build per-level data structures
            amrex::Vector<amrex::MultiFab>   mf_vec(nlevels);
            amrex::Vector<amrex::Geometry>   geom_vec(nlevels);
            amrex::Vector<int>               level_steps(nlevels);
            // ref_ratio has nlevels-1 entries (finest level has none)
            amrex::Vector<amrex::IntVect>    ref_ratio;
            ref_ratio.reserve(nlevels > 1 ? nlevels - 1 : 0);

            // Periodicity is not stored in plotfiles; assume non-periodic.
            amrex::Array<int, AMREX_SPACEDIM> is_periodic{};
            for (int d = 0; d < AMREX_SPACEDIM; ++d) {
                is_periodic[d] = 0;
            }

            for (int lev = 0; lev < nlevels; ++lev) {
                amrex::Box prob_domain = pfd.probDomain(lev);
                geom_vec[lev] = amrex::Geometry(prob_domain, real_box,
                                                coord_sys, is_periodic.data());
                level_steps[lev] = pfd.levelStep(lev);

                // ref_ratio[lev] is the ratio between level lev+1 and lev
                if (lev < finest_level) {
                    ref_ratio.push_back(pfd.refRatioVect(lev));
                }

                // Read all components for this level
                mf_vec[lev] = pfd.get(lev);
            }

            if (is_io_proc) {
                amrex::Print() << "   Levels      : " << nlevels
                               << " (finest = " << finest_level << ")\n"
                               << "   Components  : " << ncomp << "\n"
                               << "   Sim. time   : " << sim_time << "\n";
            }

            // ---------------------------------------------------------------
            // Write HDF5 plotfile
            // ---------------------------------------------------------------
#ifdef AMREX_USE_HDF5
            {
                BL_PROFILE("WriteMultiLevelPlotfileHDF5");

                amrex::Vector<const amrex::MultiFab*> mf_ptrs =
                    amrex::GetVecOfConstPtrs(mf_vec);

#if defined(AMREX_USE_HDF5_ZFP) || defined(AMREX_USE_HDF5_SZ)
                amrex::WriteMultiLevelPlotfileHDF5(
                    outfile, nlevels, mf_ptrs,
                    varnames, geom_vec,
                    sim_time, level_steps, ref_ratio,
                    hdf5_compression);
#else
                // Built without a compression plugin; ignore the compression
                // descriptor and write with default (None@0).
                if (is_io_proc && hdf5_compression != "None@0") {
                    amrex::Print()
                        << "   WARNING: AMReX was not built with ZFP/SZ support. "
                           "Writing uncompressed HDF5.\n";
                }
                amrex::WriteMultiLevelPlotfileHDF5(
                    outfile, nlevels, mf_ptrs,
                    varnames, geom_vec,
                    sim_time, level_steps, ref_ratio);
#endif
            }
#endif // AMREX_USE_HDF5

            amrex::Real t_end = amrex::ParallelDescriptor::second();

            if (is_io_proc) {
                amrex::Print() << "   Done in     : "
                               << (t_end - t_start) << " s\n\n";
            }
        } // end loop over plotfiles

        if (is_io_proc) {
            amrex::Print() << "All done.\n\n";
        }

    } // end AMReX scope
    amrex::Finalize();
    return 0;
}
