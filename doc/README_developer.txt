Publishing SCORPIO Documentation
-------------------------------
To generate and publish the SCORPIO documentation,

# Clone the SCORPIO repository to a source directory (to <SCORPIO_SRCDIR>) and create a development branch off the master branch
# Create a separate build directory (<SCORPIO_BUILDDIR>) to build the documentation
# Change the current directory to the build directory and configure SCORPIO.
# Run "make doc" to generate the documentation in the build directory

The HTML documentation is generated in the doc/html subdirectory within the build directory (<SCORPIO_BUILDDIR>/doc/html).

# Copy the html directory to the source directory to replace the current HTML documentation in the doc/html directory inside the source directory (<SCORPIO_SRCDIR>/doc/html)
# Check-in the latest changes to the html directory to the development branch (Please make sure that the doxygen source changes and the generated HTML files are checked-in in two separate commits)
# Create a PR to merge the development branch to master (and merge it to develop/master after the PR is approved)
