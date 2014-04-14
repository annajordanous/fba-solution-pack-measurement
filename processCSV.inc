<?php

module_load_include('inc', 'islandora', 'includes/tuque');

/**
 * Set up a HTML form to use to submit a file to be validated
 * For testing only.
 * 
 * @param array &$form_state
 * @return array form with submit button+array slot for uploaded 
  file
 * 
 */
function fba_solution_pack_measurement(&$form_state = array()) {

    $form = array();
// Set the form encoding type.
    $form['#attributes']['enctype'] = "multipart/form-data";

// TODO Set a maximum file size (CHECK THIS)
    /* 	$form['hidden'] = array(
      '#type' => 'hidden',
      '#name' => 'MAX_FILE_SIZE',
      '#value' =>	'100000',
      ); */

// Add a file upload file.
    $form['uploaded_file'] = array(
        '#title' => 'UploadingCSV',
        '#name' => 'uploaded_file',
        '#type' => 'file',
        '#description' => t('To validate a CSV file of data'),
    );
// Add a submit button
    $form['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Upload file'),
    );

    return $form;
}

/**
 * Checks a valid CSV function has been uploaded
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 * 
 */
function fba_solution_pack_measurement_validate($form, &$form_state) {

    $valid_CSV = false; //default init value
    if (!$form_state['form_has_errors']) {
        $valid_CSV = fba_solution_pack_measurement_check_filetype_uploaded();
        if ($valid_CSV) {
            // File is a CSV, can start to process it
            // Set files to form_state, to process when form is submitted.
            $file = file_get_contents($_FILES['uploaded_file']['tmp_name']);

            $form_state['values']['file'] = $file;

            // Processing First Row of CSV file (user defined headers)
            // ignore the first row for now - contains non-DTC-vocab headers
            $temp = explode(",", strtok($file, "\n"));

            // Processing Second Row of CSV files (vocab terms)
            // Examine the second row and check if each entry in this row  is a DTC vocab term
            $vocab_terms = explode(",", strtok("\n"));
            $vocab_terms = fba_solution_pack_measurement_trim_array_values($vocab_terms);
            // NB TODO replace the use of explode() with fgetcsv (in case cells have commas in them)

            $invalid_vocab_term_found = false;
            for ($i = 0, $j = count($vocab_terms); $i < $j; $i++) {
                $term = trim($vocab_terms[$i]);
                // Check terms. Set flag if invalid vocab term found
                $vocab_terms[$i] = fba_solution_pack_measurement_check_vocab_term(&$term);
                if ($vocab_terms[$i] == NULL) {
                    $invalid_vocab_term_found = true;
                }
            }
            $valid_CSV = !($invalid_vocab_term_found);
        }
        // TODO check all rows have the same number of entries (Is this appropriate?)
    }
    if (!$valid_CSV) {
        form_set_error('uploaded_file', "The file that has been uploaded is not valid CSV or does not conform to the DTC model");
        return;
    }
}

/**
 * Ingests the Measurement object into the repository. 
 * Creates DC, metadata, RELS_EXT datastreams.
 * Generates RELS_INT datastream for the data within the CSV file:
 * -Extracts the data and headers from the CSV file uploaded as RDF
 * -and generates RELS-INT metadata for the CSV file 
 * Also generates CSV datastream to hold the CSV file itself within the Measurement object
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_measurement_submit($form, &$form_state) {
    // File is a CSV, can start to process it
    // Set files to form_state, to process when form is submitted.
    $file = file_get_contents($_FILES['uploaded_file']['tmp_name']);
    $form_state['values']['file'] = $file;

    // Processing First Row of CSV file (user defined headers)
    // Extract to an array containing each header
    $headers = explode(",", strtok($file, "\n"));
    $headers = fba_solution_pack_measurement_trim_array_values($headers);
    // Processing Second Row of CSV files (vocab terms)
    // Extract to an array containing each vocab_term then convert to DTC vocab pid
    $vocab_terms = explode(",", strtok("\n"));
    // NB TODO replace the use of explode() with fgetcsv (in case cells have commas in them)
    $vocab_terms = fba_solution_pack_measurement_trim_array_values($vocab_terms);

    for ($i = 0, $j = count($vocab_terms); $i < $j; $i++) {
        $term = trim($vocab_terms[$i]);
        // CSV file has already been validated using the check_vocab_term function, so
        // a vocab pid will be returned for each element in vocab_terms
        $vocab_terms[$i] = fba_solution_pack_measurement_check_vocab_term(&$term);
    }

    // Processing Third Row of CSV files (measurement units)
    // Extract the third row information into an array
    $units = explode(",", strtok("\n"));
    $units = fba_solution_pack_measurement_trim_array_values($units);
    foreach ($units as $key => $value) {
        $units[$key] = fba_solution_pack_measurement_get_units($value);
    }
    
    // go through and extract the data into an array of arrays
    $current_row = explode(",", strtok("\n"));
    $data_array = array();
    while (count($current_row) > 1 && $current_row[0] !== NULL) {
        // add new data to the data array
        $data_array[] = $current_row;
        $current_row = explode(",", strtok("\n"));
    };

    $repository = fba_solution_pack_measurement_setup_repository_connection();

    $measurement_pid = fba_solution_pack_measurement_get_measurement_pid($repository, 'datac');
    // 1. create measurement object
    $new_measurement_object = fba_solution_pack_measurement_create_new_measurement_object($repository, $measurement_pid);
    // 2. Generate DC datastream [transform] and ingest into object - left out for now as this DC is generated automatically for us by Tuque
    
    // 3. Generate MADS datastream (MODS? TODO Check with Richard) [transform]
//    $ds_content = '<mads xmlns="http://www.loc.gov/mads/v2" xmlns:xlink="http://www.w3.org/1999/xlink"><authority><topic>Measurement</topic></authority>
//		<related type="broader"><topic>Data Component</topic></related>
//		</mads>'; // TODO dummy- GENERATE MADS CONTENT
//    $mads_datastream = fba_solution_pack_measurement_create_datastream($new_measurement_object, $dsid = "MADS", $controlGroup = "X", $label = "MADS Record for this object", $mimetype = "text/xml", $ds_content);
//    $new_measurement_object->ingestDatastream($mads_datastream);

    // 4. Generate CSV datastream 
    //   [TODO check image/book/PDF - generate pointer to file that is stored within repository somewhere(binary within object?)]
    $ds_content = '<csv><name>'.$_FILES['uploaded_file']['name'].'</name>
                <content>'.file_get_contents($_FILES['uploaded_file']['tmp_name']).'</content></csv>';
    // TODO Currently the CSV datastream is stored as a string but Islandora (in related sol packs) store files as new Fedora objects and link from the parent object to the new object representing the file
    // TODO NB to extract the CSV file we can use the datastream label to restore the filename and the datastream content to restore the file content
    $csv_datastream = fba_solution_pack_measurement_create_datastream($new_measurement_object, $dsid = "CSV", $controlGroup = "X", $label = $_FILES['uploaded_file']['name']." file for this object", $mimetype = "text/csv", $ds_content);
    $new_measurement_object->ingestDatastream($csv_datastream);

    // 5. Generate RELS_INT datastream [see later functions]
    $ds_content = fba_solution_pack_measurement_create_RELS($headers, $vocab_terms, $units, $data_array, $measurement_pid);
    $rels_int_datastream = fba_solution_pack_measurement_create_datastream($new_measurement_object, $dsid = "RELS_INT", $controlGroup = "X", $label = "Fedora Internal Relationships file for this object", $mimetype = "application/rdf+xml", $ds_content);
    $new_measurement_object->ingestDatastream($rels_int_datastream);

    // 6. Generate RELS_EXT datastream - left out for now as this RELS_EXT is generated automatically for us by Tuque

    // ingest object
    echo "Success. New object ingested into repository<br />";
    $object = $repository->ingestObject($new_measurement_object);
//var_dump($object);
//echo "<br />";
//$testpid = "vocab:1001";
//$test_fedora_object = islandora_object_load($testpid);
//echo "<br /><br />test FEDORA OBJECT (to test we're in the right repository): ";
//print_r('pid '.$test_fedora_object->id);
//print_r(' label '.$test_fedora_object->label);
//$measurement_fedora_object = islandora_object_load($measurement_pid);
//echo "<br /><br />NEW FEDORA OBJECT: ";
//print_r('pid '.$measurement_fedora_object->id);
//print_r(' label '.$measurement_fedora_object->label);
}

function fba_solution_pack_measurement_trim_array_values($array) {
    foreach ($array as $key => $value) {
        $array[$key] = trim($value);
    }
    return $array;
}

/**
 * Checks that a CSV file has been uploaded successfully.
 * 
 * return boolean true if a file has been uploaded successfully and it is of type csv 
 * 	(i.e. it has the extension .csv); return false otherwise
 */
function fba_solution_pack_measurement_check_filetype_uploaded() {
    $valid_CSV = true; // default value;
    // Check file was uploaded successfully as a result of a user upload
    if (!is_uploaded_file($_FILES['uploaded_file']['tmp_name'])) {
        echo "failed - File not uploaded <br />";
        $valid_CSV = false;
    } else {
        echo "File uploaded successfully. Validating vocabulary terms used in ";
        print($_FILES['uploaded_file']['name'] . " ...<br />");

        // Check file is a CSV file
        // Originally tried the line below but doesn't work for CSV files generated
        // 	through Excel's save as CSV function
        // if (strcmp($_FILES['uploaded_file']['type'],'text/csv') <>0) 

        if (strcasecmp(end(explode('.', $_FILES['uploaded_file']['name'])), 'csv') <> 0) {
            echo "This file is not a csv file, file extension is:" . end(explode('.', $_FILES['uploaded_file']['name']));
            $valid_CSV = false;
        }
    }
    return $valid_CSV;
}

/**
 * Return the next available pid in the Fedora repository for the 
 * Measurement file that is being uploaded
 * 
 * @param repository An IslandoraTuque object representing the Fedora repository
 * @param namespace default 'datac' - the namespace for the pid (optional)
 * @return the pid for the Measurement object
 */
function fba_solution_pack_measurement_get_measurement_pid($repository, $namespace = 'datac') {
    // TODO check datac:collection exists
    // get next available PID in that namespace. 
    $measurement_pid = $repository->getNextIdentifier($namespace);

    return $measurement_pid;
}

/**
 * Create a new Measurement object based on the measurement form data received
 * 
 * @param repository The current fedora repository
 * @param measurement_pid The pid for the Measurement object 
 * 
 * @return the newly created Measurement object
 */
function fba_solution_pack_measurement_create_new_measurement_object($repository, $measurement_pid) {
    $new_fedora_object = $repository->constructObject($measurement_pid);
    $new_fedora_object->models = array('fba:measurement_sub_class_cmodel');
    $new_fedora_object->label = 'Measurement Object';
    $new_fedora_object->owner = 'fedoraAdmin';

// for creating and ingesting a new fedora object
//	$new_fedora_object = islandora_add_object($fedora_object);
// to work with existing fedora object, loaded from repository
//	$new_fedora_object = islandora_object_load($measurement_pid);
    return $new_fedora_object;
}

function fba_solution_pack_measurement_create_datastream(&$fedora_object, $dsid, $controlGroup = "X", $label = "New Datastream", $mimetype, $content, $from_file = false) {    
    $new_datastream = $fedora_object->constructDatastream($dsid, $controlGroup);
    $new_datastream->label = $label;
    $new_datastream->mimetype = $mimetype;
    if ($from_file) {  // TODO can set a flag within the parameters for create_datastream, to indicate that it needs to set datastream content from a file rather than string (for CSV fix)
        $new_datastream->setContentFromFile($content);
    } else {
        $new_datastream->setContentFromString($content);
    }
    return $new_datastream;
}

/**
 * Checks validity of a term in the vocab row of a CSV file. Check:
 *  1. it starts with the right prefix 
 *  2. it is a valid vocab term in the DTC vocabulary
 *
 * @param array $term
 *   the term being checked for validity
 * @return string vocab_pid
 * 	return the pid of the $term in the DTC vocabulary, 
 * 	or NULL (if invalid term)
 */
function fba_solution_pack_measurement_check_vocab_term(&$term) {
    $vocab_pid = NULL; // default to null
    // check that each term in the vocab row starts with the right prefix
    if (!ereg("^dtc-vocab:", $term)) {
        // if a term does not start dtc-vocab, reject
        echo "$term is not a vocabulary term (missing the dtc-vocab: prefix) <br />";
    } else {
        $term = trim(substr(strstr($term, ":"), 1), " \"");
        // Check that each term in the vocab row is a valid vocab term	
        $query = 'SELECT ?s WHERE {
				?s <info:fedora/fedora-system:def/model#label> "' . $term . '" .
				?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
					<info:fedora/vocab:collection> .
			}';
        // run the query to see if current term is a vocab term. 
        // NB $limit is set to 1 as we only want to check whether $term exists in the vocabulary or not
        // TUPLE query as we aren't interested in retrieving whole triples
        $result = fba_solution_pack_measurement_risearch($query, 'TUPLE', 1, 'Sparql');
        $returnedURIs = fba_solution_pack_measurement_get_URI_from_Sparql_result($result);
        if (count($returnedURIs) <> 1) {
            // If at first the term isn't found, this could be because it 
            //   isn't capitalised the same way as in the Vocabulary.
            //   Usually vocab terms have First letter only capitalised
            //   so try converting the term to the same form and looking for that
            $ucfirstterm = ucfirst(strtolower($term));
            if (strcmp($term, $ucfirstterm) <> 0) {
                $query = 'SELECT ?s WHERE {
					?s <info:fedora/fedora-system:def/model#label> "' . $ucfirstterm . '" .
					?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
						<info:fedora/vocab:collection> .
				}';
                echo "&nbsp;&nbsp;Not finding \"$term\", trying it with capitalisation as $ucfirstterm. ";
                // run the query to see if current term is a vocab term. 
                $result = fba_solution_pack_measurement_risearch($query, 'TUPLE', 1, 'Sparql');
                $returnedURIs = fba_solution_pack_measurement_get_URI_from_Sparql_result($result);
            }
            if (count($returnedURIs) <> 1) {
                // If the term still isn't being found, try doing a case-insensitive 
                //  filtered search for the term. This is much slower than the above searches.
                //  TODO replace this search by a Solr search?						
                $query = 'SELECT ?s WHERE {
					?s <info:fedora/fedora-system:def/model#label> ?label .
					?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
						<info:fedora/vocab:collection> .
					FILTER regex(str(?label), 
"^' . $term . '$","i") 
				}';
                echo "&nbsp;&nbsp;Not finding \"$term\", trying a case insensitive search (slower). ";
//print(htmlentities($query));	
                // run the query to see if current term is a vocab term. 
                $result = fba_solution_pack_measurement_risearch($query, 'TUPLE', 1, 'Sparql');
                $returnedURIs = fba_solution_pack_measurement_get_URI_from_Sparql_result($result);
                if (count($returnedURIs) <> 1) {

                    // TODO search for partial match? Possibly Solr
                    // TODO but for now the search stops here
                    echo "<br />&nbsp;&nbsp;&nbsp;&nbsp;<b>$term is invalid vocab term</b><br />";
                } else {
                    echo "&nbsp;Case insensitive search for $term validated successfully. ";
                    echo "<em>To speed up the validation process, make sure you capitalise the DTC vocab terms as they are capitalised in the Vocabulary.</em><br />";
                    $vocab_pid = $returnedURIs[0];
                }
            } else {
                echo "&nbsp;$ucfirstterm validated successfully. ";
                echo "<em>To speed up the validation process, make sure you capitalise the DTC vocab terms as they are capitalised in the Vocabulary.</em><br />";
                $vocab_pid = $returnedURIs[0];
            }
        } else {
//			echo "<br />&nbsp;&nbsp;$term validated successfully<br />";
            $vocab_pid = $returnedURIs[0];
        }
    }
    return $vocab_pid;
}

/**
 * check if the $unit parameter starts with the 'Units:' prefix
 * (case insensitive)
 *  If it does, strip off this prefix, trim off any whitespace 
 *   surrounding the remaining string and return the result.
 *  If it doesn't start with this prefix, trim off any whitespace
 *   around the $unit parameter and return the result
 *
 * @param string $unit
 *   the string being checked for the 'units: prefix
 * @return string 
 * 	return the trimmed string
 */
function fba_solution_pack_measurement_get_units(&$unit) {
    // 
    $prefix = strtolower(substr($unit, 0, 6));
    if (ereg("^units:", $prefix)) {
         return trim(substr($unit, 6));
    } else {
         return trim($unit);
    }
}

function fba_solution_pack_measurement_setup_repository_connection() {
    $connection = new RepositoryConnection('http://localhost:8080/fedora',
                    'fedoraAdmin', 'Br1ght0nR0cks70');
    $connection->reuseConnection = TRUE;
    $repository = new FedoraRepository(new FedoraApi($connection), new SimpleCache());

//	$repository = tuque_wrapper_get_repository_instance();
    //if (strcmp(date_default_timezone_get(), 'Europe/London')<>0)  { 
    date_default_timezone_set('Europe/London');
    //}
//	$pid = "vocab:1001";
//	$test_fedora_object = islandora_object_load($pid);
//echo "<br /><br />FEDORA OBJECT (to test we're in the right repository): ";
//print_r('pid '.$test_fedora_object->id);
//print_r(' label '.$test_fedora_object->label);
    return $repository;
}

function fba_solution_pack_measurement_get_URI_from_Sparql_result($result) {
    $sxl_result = simplexml_load_string($result);
    $returnedURIs = array();
    foreach ($sxl_result->results->children() as $things) {
        $returnedURIs[] = $things->s["uri"];
    }
    return $returnedURIs;
}

/**
 * Creates RDF representation of the data and metadata in an uploaded CSV file 
 * 
 * @param $headers The header information in the first row of the uploaded CSV file
 * @param $vocab_terms The vocab terms listed in the second row of the uploaded CSV file
 * @param $units The measurement units given in the third row of the uploaded CSV file
 * @param $data_array The data contained in rows 4..n of the uploaded CSV file
 * @param @measurement_pid The pid that has been assigned to the CSV file
 *
 */
function fba_solution_pack_measurement_create_RELS($headers, $vocab_terms, $units, $data_array, $measurement_pid) {

    $returnedRDF = new SimpleXMLElement('<rdf:RDF 
		xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
		xmlns:rdfs="http://www.w3.org/2000/01/rdf-schema#" 
		xmlns:relsext="info:fedora/fedora-system:def/relations-external#" 
		xmlns:altrelsext="http://www.fedora.info/definitions/1/0/fedora-relsext-ontology.rdfs#"
                xmlns:fedora-model="info:fedora/fedora-system:def/model#"
		xmlns:dc="http://purl.org/dc/terms/"/>', NULL, FALSE, "http://www.w3.org/1999/02/22-rdf-syntax-ns#", TRUE);

    // This RDF file will have one RDF:description element, describing the CSV file.
    $desc = $returnedRDF->addChild('rdf:Description', NULL, 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');

    $desc->addAttribute('rdf:about', $measurement_pid);

    $dctype = $desc->addChild('dc:type', NULL, 'http://purl.org/dc/terms/');
    $dctype->addAttribute('rdf:resource', 'dc:Dataset', 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');
    
    $desc->addChild('dc:fileFormat', 'text/csv', 'http://purl.org/dc/terms/');
    
    // Add metadata for each column of the CSV file, as individual Constituents of the CSV file
    for ($i = 0; $i < count($vocab_terms); $i++) {

        $constituent = $desc->addChild('relsext:hasConstituent', NULL, 'info:fedora/fedora-system:def/relations-external#');
        $constituent->addAttribute('rdf:parseType', "Resource", 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');

        // add link to the vocab term corresponding to that CSV column data
        // (so the column dc:references that vocab term)
        $pointerToVocab =
                $constituent->addChild('dc:type', NULL, 'http://purl.org/dc/terms/');
        $pointerToVocab->addAttribute('rdf:resource', $vocab_terms[$i], 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');

        // column identifiers will be the CSV pid  with - and column number appended to the end
        $constituent->addChild('dc:identifier', $measurement_pid . '-' . $i, 'http://purl.org/dc/terms/');

        // add the column header given in the first row of the CSV column
        // (to set the value describing the column - relsext:HasDescription) 
        $constituent->addChild('relsext:HasDescription', $headers[$i], 'info:fedora/fedora-system:def/relations-external#');

        // add reference to measurement units being used for that column's data
        // (as the dc:format of the data)
        $constituent->addChild('dc:format', $units[$i], 'http://purl.org/dc/terms/'); //measurement units
        // data will be stored in RELSEXT as an RDF Seq object, i.e an ordered list
        // Each column of data will form one Subset of data for the CSV file
        $subset = $constituent->addChild('relsext:hasSubset', NULL, 'info:fedora/fedora-system:def/relations-external#');
        $seq = $subset->addChild('rdf:Seq', NULL, 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');

        // extract each datum from the relevant column of the data_array
        for ($j = 0; $j < count($data_array); $j++) {
            $seq->addChild('rdf:li', $data_array[$j][$i], 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');
        }
    }
    // print_r('Returned RDF:');
    // print_r(htmlentities($returnedRDF->asXML())); 
    return $returnedRDF->asXML();
}

/**
 * Generic risearch function.
 *
 * @param string $risearchstring
 *   actual string to be passed to risearch
 * @param string $type
 *   TUPLE (default) or TRIPLE
 * @param int $limit
 *   limit on number of results to return (default 1000) 
 * @param string $format
 *   Return format to return - see risearch documentation (no default set)
 *   TODO Set default
 *
 * @return string
 *   raw search return values
 */
function fba_solution_pack_measurement_risearch($risearchstring, $type = 'TUPLE', $limit = 1000, $format) {
    module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
//  $offset = 0;
//  $limit = 1000;
    // default value for $format, if not set, is N-Triples for $type='TRIPLE
    $encodedquery = htmlentities(urlencode($risearchstring));

    $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
    if ($type == 'TRIPLE') {
        $url .= "?type=triples&flush=TRUE&format=$format&limit=$limit&lang=sparql&stream=on&query=" . $encodedquery;
    } else {
        $url .= "?type=tuples&flush=TRUE&format=$format&limit=$limit&lang=sparql&stream=on&query=" . $encodedquery;
    }
    $content = trim(do_curl($url));
    return $content != '' ? $content : NULL;
}

