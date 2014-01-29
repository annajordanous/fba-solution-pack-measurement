<?php
//module_load_include('inc', 'fedora_repository', 'api/tuque');

function fba_solution_pack_uploadCSV(&$form_state = array()) {

  $form = array();
// Set the form encoding type.
  $form['#attributes']['enctype'] = "multipart/form-data"; 

// TODO Set a maximum file size (CHECK THIS)
/*	$form['hidden'] = array(
		'#type' => 'hidden',
		'#name' => 'MAX_FILE_SIZE',
		'#value' =>	'100000',
	); */

// Add a file upload file.
    $form['uploaded_file'] = array(
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
 * Implements hook_submit.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_uploadCSV_submit($form, &$form_state) {

    if (!$form_state['form_has_errors']) {
    exit();
  }
}


/**
 * Implements form validation.
 *
 * @param array $form
 *   standard form variable for a hook
 * @param array $form_state
 *   standard form state variable for a hook
 */
function fba_solution_pack_uploadCSV_validate($form, &$form_state) {
     $valid_CSV = true;
    // Check file was uploaded successfully as a result of a user upload
    if (!is_uploaded_file($_FILES['uploaded_file']['tmp_name']))  {
        echo "failed - File not uploaded <br />";
		$valid_CSV = false;
    } else {
        echo "File uploaded successfully<br />";

        // Check file is a CSV file
        // Originally tried the line below but doesn't work for CSV files generated
		// 	through Excel's save as CSV function
		// if (strcmp($_FILES['uploaded_file']['type'],'text/csv') <>0) {
		
		if (strcasecmp(end(explode('.', $_FILES['uploaded_file']['name'])),'csv')<>0) {
            echo "This file is not a csv file, file extension is:".end(explode('.', 
$_FILES['uploaded_file']['name']));
			$valid_CSV = false;
		} else {
            // File is a CSV, can start to process it

			// Set files to form_state, to process when form is submitted.
      		$form_state['values']['file'] = $file;

            $file = file_get_contents($_FILES['uploaded_file']['tmp_name']);
			
			// ignore the first row for now - contains non-DTC-vocab headers
			// Just convert it into an array containing each header
            $headers = explode(",", strtok($file, "\n"));
			// Examine the second row and check if each entry in this row  is a DTC vocab term
            $vocab_terms = explode(",", strtok("\n"));

			// NB TODO replace the use of explode() with fgetcsv (in case cells have commas in them)
			
            for ($i=0, $j=count($vocab_terms); $i<$j; $i++)  {
                $term = trim($vocab_terms[$i]);
		fba_solution_pack_uploadCSV_check_vocab_term(&$term, &$valid_CSV);
	        }
            }
	}
	// TODO check all rows have the same number of entries (Is this appropriate?)
	if ($valid_CSV) { 
		echo "<br />File validated successfully<br /><br />"; 
		// File is valid - we can now extract the data from the CSV file and store as RDF
		// Extract the third row information into an array
		$units = explode(",", strtok("\n"));

		// go through and extract the data into an array of arrays
		$current_row = explode(",", strtok("\n"));
		$data_array = array();
		while (count($current_row)>1 && $current_row[0] !== NULL) {
		// add new data to the data array
		$data_array[] = $current_row;
			$current_row = explode(",", strtok("\n"));
		} ;
	$CSV_pid = fba_solution_pack_uploadCSV_get_CSV_pid();
	$returnedRDF = fba_solution_pack_uploadCSV_create_RELS($headers, $vocab_terms, $units, $data_array, $CSV_pid);

	echo "<br />Returned_RDF:<br />";
	print(htmlentities($returnedRDF->asXML()));

	fba_solution_pack_uploadCSV_update_datastream($returnedRDF);
	} else {
		echo "<br />File was NOT validated successfully<br /><br />";
	}
}

/** 
 * Identify the pid in the Fedora repository for the CSV file that is being uploaded
 * 
 * @return the pid for the CSV file
 */

function fba_solution_pack_uploadCSV_get_CSV_pid()  {
	//TODO how to find out/generate the pid for the CSV file? currently initialised as a random pid
	$CSV_pid = 'info:fedora/dtc:123';
	return $CSV_pid;
}

/** 
 * Updates the RELS (INT? EXT) datastream with the relations in the parameter RDF file
 * 
 * @param returnedRDF An RDF file containing the relations that will be added to the RELS datastream
 */
function fba_solution_pack_uploadCSV_update_datastream($returnedRDF) {
}


/**
 * Checks that each term in the vocab row of a CSV file starts with the right prefix and is a valid vocab term
 *
 * @param array $term
 *   the term being checked for validity
 * @param array $valid_CSV
 *   standard valid_CSV boolean variable for whether the CSV file has been found valid or not
 */
function fba_solution_pack_uploadCSV_check_vocab_term(&$term, &$valid_CSV)  {
	// check that each term in the vocab row starts with the right prefix
	if (!ereg("^dtc-vocab:", $term))   {
		// if a term does not start dtc-vocab, reject
		$valid_CSV = false;                     
		echo "$term is not a vocabulary term (missing the dtc-vocab: prefix) <br />";
	} else {
		$term=trim(substr(strstr($term, ":"), 1), " \"");
		// Check that each term in the vocab row is a valid vocab term	
		$query = 'SELECT ?s WHERE {
				?s <info:fedora/fedora-system:def/model#label> "'.$term.'" .
				?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
					<info:fedora/vocab:collection> .
			}'; 
		// run the query to see if current term is a vocab term. 
		// NB $limit is set to 1 as we only want to check whether $term exists in the vocabulary or not
		// TUPLE query as we aren't interested in retrieving whole triples
		$result = uploadCSV_risearch($query, 'TUPLE', 1, 'Sparql'); 
		$returnedURIs = fba_solution_pack_uploadCSV_get_URI_from_Sparql_result($result);
		if (count($returnedURIs) <>1) {
			// If at first the term isn't found, this could be because it 
			//   isn't capitalised the same way as in the Vocabulary.
			//   Usually vocab terms have First letter only capitalised
			//   so try converting the term to the same form and looking for that
			$ucfirstterm = ucfirst(strtolower($term));
			if (strcmp($term, $ucfirstterm) <>0)  { 
				$query = 'SELECT ?s WHERE {
					?s <info:fedora/fedora-system:def/model#label> "'.$ucfirstterm.'" .
					?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
						<info:fedora/vocab:collection> .
				}'; 
				echo "Not finding \"$term\", trying it with capitalisation as $ucfirstterm. ";
				// run the query to see if current term is a vocab term. 
				$result = uploadCSV_risearch($query, 'TUPLE', 1, 'Sparql'); 
				$returnedURIs = fba_solution_pack_uploadCSV_get_URI_from_Sparql_result($result);
			}
		if (count($returnedURIs) <>1) {
				// If the term still isn't being found, try doing a case-insensitive 
				//  filtered search for the term. This is much slower than the above searches.
				//  TODO replace this search by a Solr search?						
				$query = 'SELECT ?s WHERE {
					?s <info:fedora/fedora-system:def/model#label> ?label .
					?s <info:fedora/fedora-system:def/relations-external#isMemberOfCollection> 
						<info:fedora/vocab:collection> .
					FILTER (regex(str(?label), "'.$term.'", -i)); 
				}'; 
				echo "Not finding \"$term\", trying a case insensitive search (slower). ";
				// run the query to see if current term is a vocab term. 
				$result = uploadCSV_risearch($query, 'TUPLE', 1, 'Sparql'); 
				if ($result == 0) {
					// TODO search for partial match? Possibly Solr
					// TODO but for now the search stops here
					echo "<br />$term is invalid vocab term<br />";
					$valid_CSV = false;
				} else {
					echo "<br />Case insensitive search for $term validated successfully<br />";
					echo "To speed up the validation process, make sure you capitalise the 
							DTC vocab terms as they are capitalised in the CVocabulary.<br />";
				}
			} else {
				echo "<br />$ucfirstterm validated successfully<br />";
			}
		} else {
			echo "<br />$term validated successfully<br />";
		}
	}
	// TODO NB currently I'm using the term as in the CSV even if it is capitalised wrongly
	
}

function fba_solution_pack_uploadCSV_get_URI_from_Sparql_result($result)  {
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
 * @param @CSV_pid The pid that has been assigned to the CSV file
 *
 */
function fba_solution_pack_uploadCSV_create_RELS($headers, $vocab_terms, $units, $data_array, $CSV_pid)  {

	$returnedRDF = new SimpleXMLElement('<rdf:RDF 
		xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
		xmlns:rdfs="http://www.w3.org/2000/01/rdf-schema#" 
		xmlns:relsext="info:fedora/fedora-system:def/relations-external#" 
		xmlns:altrelsext="http://www.fedora.info/definitions/1/0/fedora-relsext-ontology.rdfs#"
		xmlns:dc="http://purl.org/dc/elements/1.1/"/>', NULL, FALSE, "http://www.w3.org/1999/02/22-rdf-syntax-ns#", TRUE);

	// This RDF file will have one RDF:description element, describing the CSV file.
	$desc = $returnedRDF->addChild('rdf:Description', NULL, 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');

	$desc->addAttribute('rdf:about', $CSV_pid);

	// Add metadata for each column of the CSV file, as individual Constituents of the CSV file
	for ($i=0; $i < count($vocab_terms); $i++) {

		$constituent = $desc->addChild('relsext:hasConstituent', NULL, 
'info:fedora/fedora-system:def/relations-external#');
		$constituent->addAttribute('rdf:parseType', "Resource", 
'http://www.w3.org/1999/02/22-rdf-syntax-ns#');

		// column identifiers will be the CSV pid  with - and column number appended to the end
		$constituent->addChild('dc:identifier', $CSV_pid.'-'.$i, 'http://purl.org/dc/elements/1.1/');

		// add the column header given in the first row of the CSV column
		// (to set the value describing the column - relsext:HasDescription) 
		$constituent->addChild('relsext:HasDescription', $headers[$i], 
'info:fedora/fedora-system:def/relations-external#');
		
		// add link to the vocab term corresponding to that CSV column data
		// (so the column dc:references that vocab term)
		$pointerToVocab = 
			$constituent->addChild('dc:references', NULL, 'http://purl.org/dc/elements/1.1/');
		$pointerToVocab->addAttribute('rdf:resource','info:fedora/'.$vocab_terms[$i], 
'http://www.w3.org/1999/02/22-rdf-syntax-ns#');
	
		// add reference to measurement units being used for that column's data
		// (as the dc:format of the data)
		$constituent->addChild('dc:format', 'N/A', 'http://purl.org/dc/elements/1.1/'); //measurement units
		
		// data will be stored in RELSEXT as an RDF Seq object, i.e an ordered list
		// Each column of data will form one Subset of data for the CSV file
		$subset = $constituent->addChild('relsext:hasSubset', NULL, 
'info:fedora/fedora-system:def/relations-external#');
		$seq = $subset->addChild('rdf:Seq', NULL, 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');

		// extract each datum from the relevant column of the data_array
		for ($j=0; $j<count($data_array); $j++)  {
echo $j;
			$seq->addChild('rdf:li', $data_array[$j][$i], 'http://www.w3.org/1999/02/22-rdf-syntax-ns#');
		}
	}

	return $returnedRDF;
}


/**
 * First attempt at generic risearch function.
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
function uploadCSV_risearch($risearchstring, $type = 'TUPLE', $limit=1000, $format) {
  module_load_include('inc', 'fedora_repository', 'api/fedora_utils');
//  $offset = 0;
//  $limit = 1000;
  // default value for $format, if not set, is N-Triples for $type='TRIPLE
  $encodedquery = htmlentities(urlencode($risearchstring));

  $url = variable_get('fedora_repository_url', 'http://localhost:8080/fedora/risearch');
  if ($type == 'TRIPLE') {
    $url .= "?type=triples&flush=TRUE&format=$format&limit=$limit&lang=sparql&stream=on&query=" . $encodedquery;
  }
  else {
    $url .= "?type=tuples&flush=TRUE&format=$format&limit=$limit&lang=sparql&stream=on&query=" . $encodedquery;
  }
  $content = trim(do_curl($url));
  return $content != '' ? $content : NULL;
}

